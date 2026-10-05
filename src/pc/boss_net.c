/* One server-granted native simulator per boss. The server can retain the
 * canonical state while its own player is in another area. A voluntary PASS
 * freezes the old simulator before submitting its final state; a new epoch is
 * granted only after that state is accepted. Ordinary object packets never
 * overwrite this channel. Offline behavior bypasses the channel completely. */
#include "boss_net.h"
#include "boss_net_protocol.h"
#include "network/network.h"
#include "cliopts.h"
#include "utils/misc.h"
#include "behavior_data.h"
#include "behavior_table.h"
#include "model_ids.h"
#include "object_fields.h"
#include "object_constants.h"
#include "game/area.h"
#include "game/level_update.h"
#include "game/object_helpers.h"
#include "game/object_list_processor.h"
#include "game/mario.h"
#include "game/mario_misc.h"
#include "game/obj_behaviors.h"
#include <math.h>
#include <string.h>
#ifdef ROCKET_CAR_QA
#include <stdio.h>
#include "game/camera.h"
#include "game/ingame_menu.h"
#endif

#define BOSS_ROOMS 64
#define OBJECT_BODY_MAX 1100
#define WHOMP_COIN_BODY (42+4*OBJECT_NUM_FIELDS)
typedef struct BossRecord {
    BossNetKey key;
    BossNetLease lease;
    BossNetLedger source_events,deliveries;
    unsigned char state[OBJECT_BODY_MAX],reward[BOSS_NET_BODY];
    uint16_t state_size,reward_size,local_token,members[MAX_PLAYERS];
    uint8_t present[MAX_PLAYERS],used,frozen,pass_to,reward_seen,reward_reserved;
    uint32_t outgoing,event_out,applied_epoch,applied_revision,barrier,frame;
    uint8_t pending_owner,deliveries_ready,admitted;
    uint32_t delivery_out;
    /* Native small Whomps allow five step coins plus five on ground-pound.
     * Hold authenticated deliveries through loading, never across re-entry. */
    uint8_t pending_coins[10][WHOMP_COIN_BODY],pending_coin_count;
    struct Object *object;
    double joined;
} BossRecord;
static BossRecord records[BOSS_ROOMS];
static uint32_t next_epoch;
static int applying;

static void clear_record(BossRecord *r){
    BossNetKey key=r->key;
    boss_net_ledger_reset(&r->source_events,0);boss_net_ledger_reset(&r->deliveries,0);
    memset(r,0,sizeof(*r));r->used=1;r->key=key;
    r->lease.owner=r->pass_to=r->pending_owner=BOSS_NET_NO_OWNER;
}

int boss_net_enabled(void){return gCLIOpts.characterNet&&gNetworkType!=NT_NONE&&gNetworkPlayerLocal;}
static unsigned boss_kind(const struct Object *o){
    return !o?0:o->behavior==bhvKingBobomb?1:o->behavior==bhvBowser?2:
        o->behavior==bhvWhompKingBoss?3:o->behavior==bhvSmallWhomp?4:0;
}
int boss_net_managed(const struct Object *o){return boss_net_enabled()&&boss_kind(o);}
static BossNetKey object_key(const struct Object *o){
    BossNetKey k={o->oSyncID,(uint16_t)gCurrLevelNum,(uint8_t)gCurrCourseNum,
        (uint8_t)gCurrActStarNum,(uint8_t)gCurrAreaIndex,(uint8_t)boss_kind(o)};return k;
}
static BossRecord *find_record(const BossNetKey *key,int create){
    BossRecord *free_record=NULL;
    for(unsigned i=0;i<BOSS_ROOMS;i++){
        if(records[i].used&&boss_net_key_equal(key,&records[i].key))return &records[i];
        if(!records[i].used&&!free_record)free_record=&records[i];
    }
    if(!create||!free_record)return NULL;
    memset(free_record,0,sizeof(*free_record));free_record->used=1;free_record->key=*key;
    free_record->lease.owner=BOSS_NET_NO_OWNER;free_record->pass_to=free_record->pending_owner=BOSS_NET_NO_OWNER;
    return free_record;
}
static BossRecord *for_object(const struct Object *o){
    if(!boss_net_managed(o)||!o->oSyncID)return NULL;
    BossNetKey key=object_key(o);return find_record(&key,0);
}
static int in_room(const BossNetKey *k,const struct NetworkPlayer *np){
    return np&&np->connected&&np->currCourseNum==k->course&&np->currActNum==k->act&&
        np->currLevelNum==k->level&&np->currAreaIndex==k->area;
}
static int member(BossRecord *r,unsigned global){
    if(global>=MAX_PLAYERS||!r->present[global])return 0;
    struct NetworkPlayer *np=network_player_from_global_index(global);
    return in_room(&r->key,np)&&np->currLevelAreaSeqId==r->members[global];
}
static int any_occupant(BossRecord *r){
    for(unsigned i=0;i<MAX_PLAYERS;i++){
        struct NetworkPlayer *np=&gNetworkPlayers[i];
        if(!in_room(&r->key,np))continue;
        // An already joined participant announces unload with a new token and
        // sync=false before its location changes (including same-area reload).
        // A new loading participant has no prior membership and keeps the room.
        unsigned global=np->globalIndex;
        if(global<MAX_PLAYERS&&r->present[global]&&!np->currAreaSyncValid&&
           r->members[global]!=np->currLevelAreaSeqId)continue;
        return 1;
    }
    return 0;
}
static void bind_area(BossRecord *r){
    if(!in_room(&r->key,gNetworkPlayerLocal))return;
    if(r->local_token!=gNetworkPlayerLocal->currLevelAreaSeqId){
        // A client presents a new native area instance. Old local state must
        // never mutate it before the server admits this specific entry token.
        // The server retains the room record while another member remains.
        if(gNetworkType==NT_CLIENT)clear_record(r);
        r->local_token=gNetworkPlayerLocal->currLevelAreaSeqId;r->applied_epoch=0;
        r->reward_seen=r->reward_reserved=r->admitted=0;r->joined=-10;
        r->pending_coin_count=0;
    }
}
int boss_net_simulates(const struct Object *o){
    if(!boss_net_managed(o))return 1;
    BossRecord *r=for_object(o);
    return r&&r->admitted&&r->lease.epoch&&r->applied_epoch==r->lease.epoch&&
        r->lease.owner==gNetworkPlayerLocal->globalIndex&&!r->frozen&&
        r->object==o&&r->local_token==gNetworkPlayerLocal->currLevelAreaSeqId&&gNetworkAreaLoaded;
}
uint32_t boss_net_epoch(const struct Object *o){BossRecord *r=for_object(o);return r?r->lease.epoch:0;}
int boss_net_applying(void){return applying;}
int boss_net_player_holds(struct Object *o,unsigned global){
    if(!boss_net_managed(o))return 1;
    BossRecord *r=for_object(o);
    /* A player pose may present a canonical hold, never create one or reverse a
     * newer throw. Both the owner and canonical phase must still agree. */
    return r&&r->lease.owner==global&&!r->frozen&&o->oHeldState==HELD_HELD;
}
int boss_net_reward_available(struct Object *o){
    BossRecord *r=for_object(o);return !r||(!r->reward_size&&!r->reward_seen&&!r->reward_reserved);
}
void boss_net_reset(void){
    for(unsigned i=0;i<BOSS_ROOMS;i++){
        boss_net_ledger_reset(&records[i].source_events,0);boss_net_ledger_reset(&records[i].deliveries,0);
    }
    memset(records,0,sizeof(records));next_epoch=0;applying=0;
}

static void send_message(BossRecord *r,uint8_t op,uint8_t owner,uint32_t revision,
                         const void *body,uint16_t size,int destination,int reliable){
    if(gNetworkType==NT_SERVER&&destination<0){
        for(unsigned i=1;i<MAX_PLAYERS;i++)if(in_room(&r->key,&gNetworkPlayers[i]))
            send_message(r,op,owner,revision,body,size,i,reliable);
        return;
    }
    BossNetMessage m={r->key,r->lease.epoch,revision,0,size,op,owner};
    if(gNetworkPlayerLocal)m.token=gNetworkPlayerLocal->currLevelAreaSeqId;
    if(gNetworkType==NT_SERVER&&destination>0&&destination<MAX_PLAYERS)
        m.token=gNetworkPlayers[destination].currLevelAreaSeqId;
    uint8_t wire[BOSS_NET_HEADER+BOSS_NET_BODY];
    int length=boss_net_pack(wire,sizeof(wire),&m,body);if(!length)return;
    struct Packet p={0};packet_init(&p,PACKET_BOSS_STATE,reliable,PLMT_NONE);
    packet_write(&p,wire,length);
    if(destination<0)network_send(&p);else network_send_to((u8)destination,&p);
}
static void grant_packet(BossRecord *r,int destination){
    if(destination<0){
        for(unsigned i=1;i<MAX_PLAYERS;i++)if(in_room(&r->key,&gNetworkPlayers[i]))grant_packet(r,i);
        return;
    }
    uint8_t body[OBJECT_BODY_MAX+4];memcpy(body,r->state,r->state_size);
    for(int i=0;i<4;i++)body[r->state_size+i]=(uint8_t)(r->delivery_out>>(8*i));
    send_message(r,BN_GRANT,r->lease.owner,r->lease.revision,body,r->state_size+4,destination,1);
    if(r->reward_size)send_message(r,BN_REWARD,r->lease.owner,0,r->reward,r->reward_size,destination,1);
}
static void grant(BossRecord *r,unsigned owner){
    if(++next_epoch==0)++next_epoch;
    boss_net_lease_grant(&r->lease,(uint8_t)owner,next_epoch);
    boss_net_ledger_reset(&r->source_events,0);
    r->frozen=0;r->outgoing=r->event_out=0;r->pass_to=r->pending_owner=BOSS_NET_NO_OWNER;
    r->admitted=member(r,gNetworkPlayerLocal->globalIndex);
    r->applied_epoch=0;grant_packet(r,-1);
#ifdef ROCKET_CAR_QA
    fprintf(stderr,"BOSS_NET_GRANT level=%u sync=%u epoch=%u owner=%u cached=%u reward=%u\n",
        r->key.level,r->key.sync,r->lease.epoch,r->lease.owner,r->state_size,r->reward_size);
#endif
}
static int state_valid(const BossRecord *r,const uint8_t *body,unsigned size){
    if(size<13+4*OBJECT_NUM_FIELDS+1||size>OBJECT_BODY_MAX)return 0;
    uint32_t sync,behavior;memcpy(&sync,body+1,4);memcpy(&behavior,body+9,4);
    const BehaviorScript *expected=r->key.kind==1?bhvKingBobomb:r->key.kind==2?bhvBowser:
        r->key.kind==3?bhvWhompKingBoss:bhvSmallWhomp;
    if(sync!=r->key.sync||get_behavior_from_id(behavior)!=expected)return 0;
    struct Object v={0};memcpy(v.rawData.asU32,body+13,4*OBJECT_NUM_FIELDS);
    if(v.oSyncID!=sync||v.oHeldState>HELD_DROPPED||v.oHealth<0||v.oHealth>100||v.oAction<0||
       v.oAction>(r->key.kind==1?8:r->key.kind==2?20:9)||v.oSubAction<0||v.oSubAction>100||v.oTimer<0)return 0;
    for(unsigned i=0;i<3;i++)if(!isfinite((&v.oPosX)[i])||fabsf((&v.oPosX)[i])>131072||
        !isfinite((&v.oVelX)[i])||fabsf((&v.oVelX)[i])>100000)return 0;
    if(r->key.kind>=3){
        unsigned at=13+4*OBJECT_NUM_FIELDS;
        /* Whomp extras: render flags, lifecycle flags, scale, then the five
         * original native fields. No arbitrary extra layout reaches apply. */
        if(size!=at+1+36||body[at]!=8)return 0;
        const uint8_t *extra=body+at+1;
        for(unsigned i=0;i<3;i++){
            float scale;memcpy(&scale,extra+4+4*i,4);
            if(!isfinite(scale)||scale<=0||scale>4)return 0;
        }
        if(memcmp(extra+16,&v.oAngleVelPitch,4)||memcmp(extra+20,&v.oFaceAnglePitch,4)||
           memcmp(extra+24,&v.oForwardVel,4)||memcmp(extra+28,&v.oHealth,4)||memcmp(extra+32,&v.oFaceAnglePitch,4))return 0;
    }
    return 1;
}
static void apply_state(BossRecord *r){
    bind_area(r);
    if(!r->admitted)return;
    if(gNetworkPlayerLocal&&r->lease.owner==gNetworkPlayerLocal->globalIndex&&r->applied_epoch==r->lease.epoch)return;
    if(!r->state_size&&r->lease.epoch){r->applied_epoch=r->lease.epoch;return;}
    if(!r->state_size||!gNetworkAreaLoaded||!in_room(&r->key,gNetworkPlayerLocal))return;
    struct SyncObject *so=sync_object_get(r->key.sync);
    if(!so||!so->o||boss_kind(so->o)!=r->key.kind||!sync_object_is_initialized(so->id))return;
    unsigned at=13+4*OBJECT_NUM_FIELDS;
    if(r->state[at++]!=so->extraFieldCount)return; // child field registration is still loading
    for(unsigned i=0;i<so->extraFieldCount;i++)at+=so->extraFieldsSizeBytes[i];
    if(at!=r->state_size)return;
    if(r->object==so->o&&r->applied_epoch==r->lease.epoch&&r->applied_revision==r->lease.revision)return;
    struct Packet p={0};p.packetType=PACKET_OBJECT;p.dataLength=r->state_size;p.reliable=false;
    memcpy(p.buffer,r->state,r->state_size);
    struct NetworkPlayer *np=network_player_from_global_index(r->lease.owner);
    p.localIndex=np?np->localIndex:0;
    applying++;network_receive_object(&p);applying--;
    if(p.error)return;
    r->object=so->o;r->applied_epoch=r->lease.epoch;r->applied_revision=r->lease.revision;
    if(so->o->oHeldState==HELD_HELD&&np)so->o->heldByPlayerIndex=np->localIndex;
}
static void effect_apply(BossRecord *r,const uint8_t *body,unsigned size,int reward){
    if(!reward&&r->key.kind==4&&size==WHOMP_COIN_BODY&&in_room(&r->key,gNetworkPlayerLocal)&&
       (!gNetworkAreaLoaded||!gMarioStates[0].marioObj)) {
        bind_area(r);
        if(r->pending_coin_count<10)memcpy(r->pending_coins[r->pending_coin_count++],body,size);
        return;
    }
    if(size<10||size> BOSS_NET_BODY||!gNetworkAreaLoaded||!in_room(&r->key,gNetworkPlayerLocal))return;
    bind_area(r);
    if(reward&&r->reward_seen)return;
    struct Packet p={0};p.dataLength=size;p.cursor=3;memcpy(p.buffer,body,size);
    if(!packet_initial_read(&p)||!p.levelAreaMustMatch||p.courseNum!=r->key.course||p.actNum!=r->key.act||
       p.levelNum!=r->key.level||p.areaIndex!=r->key.area)return;
    if(p.packetType!=PACKET_SPAWN_OBJECTS&&p.packetType!=PACKET_SPAWN_STAR)return;
    if(!reward&&r->key.kind==4){
        /* The independently identified native coin outlives its dead parent.
         * Authentication already checked the original parent. Reuse the
         * native area's self-parent fallback for a reordered terminal state. */
        struct SyncObject *parent=sync_object_get(r->key.sync);
        if(!parent||!parent->o||parent->forgetting){
            uint32_t detached=UINT32_MAX;memcpy(p.buffer+12,&detached,4);
        }
    }
    if(reward){
        /* Native spawn readers return without setting p.error when their
         * parent has not loaded. Keep the canonical reward pending so the
         * boss loop can retry after object registration, including re-entry. */
        if(p.packetType==PACKET_SPAWN_STAR){
            if(!gMarioStates[0].marioObj)return;
        }else{
            if(size<16)return;
            uint32_t parent;memcpy(&parent,body+12,sizeof parent);
            if(parent==UINT32_MAX){
                if(!gMarioStates[0].marioObj)return;
            }else{
                struct SyncObject *so=sync_object_get(parent);
                if(!so||!so->o)return;
            }
        }
    }
    if(reward)r->reward_seen=1;
    applying++;packet_process(&p);applying--;
}
static int float_bytes_valid(const uint8_t *bytes,unsigned count,float limit){
    for(unsigned i=0;i<count;i++){float value;memcpy(&value,bytes+4*i,4);if(!isfinite(value)||fabsf(value)>limit)return 0;}
    return 1;
}
static int effect_valid(const BossRecord *r,const uint8_t *body,unsigned size,int reward){
    if(size<10||size>BOSS_NET_BODY||body[3]!=1||body[5]!=r->key.course||body[6]!=r->key.act||
       (body[7]|((unsigned)body[8]<<8))!=r->key.level||body[9]!=r->key.area)return 0;
    if(body[0]==PACKET_SPAWN_STAR){
        return reward&&(r->key.kind==1||r->key.kind==3)&&size==52&&body[10]==0&&float_bytes_valid(body+11,3,131072)&&
            (body[27]==255||body[27]<MAX_PLAYERS)&&float_bytes_valid(body+28,6,131072);
    }
    if(body[0]!=PACKET_SPAWN_OBJECTS||(r->key.kind!=2&&r->key.kind!=4)||size<11)return 0;
    unsigned count=body[10],item_size=31+4*OBJECT_NUM_FIELDS;
    if(!count||count>4||size!=11+count*item_size||(reward&&count!=1))return 0;
    for(unsigned i=0;i<count;i++){
        const uint8_t *item=body+11+i*item_size;uint32_t parent,behavior,model;
        memcpy(&parent,item+1,4);memcpy(&model,item+5,4);memcpy(&behavior,item+9,4);
        if(!item[0]||model>=256||(i&&parent!=UINT32_MAX&&parent>=i))return 0;
        const BehaviorScript *b=get_behavior_from_id(behavior);
        if(r->key.kind==4){if(reward||count!=1||parent!=r->key.sync||b!=bhvSingleCoinGetsSpawned||model!=MODEL_YELLOW_COIN)return 0;}
        else if(reward){if(b!=bhvBowserKey&&b!=bhvGrandStar)return 0;}
        else if(b!=bhvFlameMovingForwardGrowing&&b!=bhvBowserShockWave&&b!=bhvBlueBowserFlame)return 0;
        struct Object v={0};memcpy(v.rawData.asU32,item+15,4*OBJECT_NUM_FIELDS);
        if(r->key.kind==4&&(v.oSyncID<SYNC_ID_BLOCK_SIZE||v.oSyncID>=(MAX_PLAYERS+1)*SYNC_ID_BLOCK_SIZE))return 0;
        if(!float_bytes_valid((const uint8_t*)&v.oPosX,7,131072)||
           !float_bytes_valid(item+15+4*OBJECT_NUM_FIELDS,3,1000))return 0;
        if(item[item_size-4]>1|| (item[item_size-3]!=255&&item[item_size-3]>=MAX_PLAYERS)||
           item[item_size-2]!=255||item[item_size-1]!=255)return 0; // no custom models/mods
    }
    return 1;
}
static int physical_sender(struct Packet *p){
    if(p->requestBroadcast||p->levelAreaMustMatch||p->levelMustMatch||p->orderedGroupId||
       p->localIndex>=MAX_PLAYERS||!gNetworkPlayers[p->localIndex].connected)return -1;
    if(gNetworkType==NT_CLIENT){
        if(!gNetworkPlayerServer||p->localIndex!=gNetworkPlayerServer->localIndex)return -1;
        if(p->destGlobalId!=PACKET_DESTINATION_BROADCAST&&p->destGlobalId!=gNetworkPlayerLocal->globalIndex)return -1;
        return 0;
    }
    return gNetworkPlayers[p->localIndex].globalIndex;
}
static void accept_server(BossRecord *r,const BossNetMessage *m,const uint8_t *body,unsigned sender,int local){
    if(m->op==BN_JOIN){
        struct NetworkPlayer *np=network_player_from_global_index(sender);
        if(!in_room(&m->key,np)||np->currLevelAreaSeqId!=m->token)return;
        r->present[sender]=1;r->members[sender]=m->token;
        if(local)r->admitted=1;
        if(!member(r,r->lease.owner))grant(r,sender);
        else if(!local)grant_packet(r,np->localIndex);
        return;
    }
    if(!member(r,sender)||m->token!=r->members[sender])return;
    if(m->op==BN_STATE||m->op==BN_PASS){
        unsigned state_size=m->length;
        uint32_t barrier=0;
        if(m->op==BN_PASS){if(state_size<4)return;state_size-=4;
            for(int i=0;i<4;i++)barrier|=(uint32_t)body[state_size+i]<<(i*8);}
        if(m->op==BN_PASS&&barrier<r->source_events.highest)return;
        if(!state_valid(r,body,state_size)||
           !boss_net_lease_state(&r->lease,sender,m->epoch,m->revision))return;
        int consequence=0;
        if(r->key.kind>=3){
            struct Object before={0},after={0};
            if(r->state_size)memcpy(before.rawData.asU32,r->state+13,4*OBJECT_NUM_FIELDS);
            memcpy(after.rawData.asU32,body+13,4*OBJECT_NUM_FIELDS);
            consequence=!r->state_size||before.oHealth!=after.oHealth||before.oNumLootCoins!=after.oNumLootCoins||
                (r->key.kind==4&&body[14+4*OBJECT_NUM_FIELDS+2]==0&&body[14+4*OBJECT_NUM_FIELDS+3]==0);
        }
        memcpy(r->state,body,state_size);r->state_size=state_size;
        if(m->op==BN_PASS){r->pending_owner=member(r,m->owner)?m->owner:sender;r->barrier=barrier;
            if(r->source_events.contiguous==barrier&&r->source_events.highest==barrier)grant(r,r->pending_owner);}
        else send_message(r,BN_STATE,r->lease.owner,r->lease.revision,body,m->length,-1,consequence);
        if(!local||m->op==BN_PASS)apply_state(r);
    }else if(m->op==BN_EFFECT||m->op==BN_REWARD){
        if(sender!=r->lease.owner||m->epoch!=r->lease.epoch||
           (r->pending_owner!=BOSS_NET_NO_OWNER&&m->revision>r->barrier)||
           !effect_valid(r,body,m->length,m->op==BN_REWARD)||
           !boss_net_ledger_accept(&r->source_events,m->revision))return;
        r->lease.event=r->source_events.contiguous;
        if(m->op==BN_REWARD){
            if(!r->reward_size){memcpy(r->reward,body,m->length);r->reward_size=m->length;}
            r->reward_reserved=0;
            send_message(r,BN_REWARD,r->lease.owner,0,r->reward,r->reward_size,-1,1);
            if(!local)effect_apply(r,r->reward,r->reward_size,1);
        }else{
            send_message(r,BN_EFFECT,r->lease.owner,++r->delivery_out,body,m->length,-1,1);
            if(!local)effect_apply(r,body,m->length,0);
        }
        if(r->pending_owner!=BOSS_NET_NO_OWNER&&r->source_events.contiguous==r->barrier&&
           r->source_events.highest==r->barrier)grant(r,r->pending_owner);
    }
}
void boss_net_receive(struct Packet *p){
    if(!boss_net_enabled())return;
    int sender=physical_sender(p);if(sender<0)return;
    BossNetMessage m;
    if(p->cursor>p->dataLength||!boss_net_unpack(&m,p->buffer+p->cursor,p->dataLength-p->cursor))return;
    if(gNetworkType==NT_CLIENT&&(!in_room(&m.key,gNetworkPlayerLocal)||
       m.token!=gNetworkPlayerLocal->currLevelAreaSeqId))return;
    const uint8_t *body=p->buffer+p->cursor+BOSS_NET_HEADER;
    BossRecord *r=find_record(&m.key,gNetworkType==NT_SERVER?m.op==BN_JOIN:1);if(!r)return;
    if(gNetworkType==NT_SERVER){accept_server(r,&m,body,(unsigned)sender,0);return;}
    bind_area(r);
    if(m.op==BN_GRANT){
        if(!in_room(&m.key,gNetworkPlayerLocal)||m.token!=gNetworkPlayerLocal->currLevelAreaSeqId)return;
        bind_area(r);
        if(m.owner>=MAX_PLAYERS&&m.owner!=BOSS_NET_NO_OWNER)return;
        if(m.length<4)return;
        m.length-=4;
        uint32_t baseline=0;for(int i=0;i<4;i++)baseline|=(uint32_t)body[m.length+i]<<(8*i);
        if(m.length&&!state_valid(r,body,m.length))return;
        uint32_t old=r->lease.epoch;
        if(!boss_net_lease_grant(&r->lease,m.owner,m.epoch))return;
        r->admitted=1;
        if(!r->deliveries_ready){boss_net_ledger_baseline(&r->deliveries,baseline);r->deliveries_ready=1;}
        if(old!=m.epoch){r->frozen=0;r->outgoing=r->event_out=0;r->pass_to=BOSS_NET_NO_OWNER;}
        if(old!=m.epoch||boss_net_newer(m.revision,r->lease.revision)){
            r->lease.revision=m.revision;r->state_size=m.length;if(m.length)memcpy(r->state,body,m.length);
        }
        // Child fields may finish registering after GRANT. Retry the newest
        // cached state without allowing an older GRANT to rewind its revision.
        apply_state(r);
    }else if(m.op==BN_STATE){
        if(!state_valid(r,body,m.length)||!boss_net_lease_state(&r->lease,m.owner,m.epoch,m.revision))return;
        if(m.owner!=gNetworkPlayerLocal->globalIndex){memcpy(r->state,body,m.length);r->state_size=m.length;apply_state(r);}
    }else if(m.op==BN_REWARD||m.op==BN_EFFECT){
        /* These are already committed by the authenticated server. Delivery
         * is independent of lease epochs, so GRANT cannot discard an older
         * flame packet still in flight. Rewards have one canonical identity. */
        if(!effect_valid(r,body,m.length,m.op==BN_REWARD))return;
        if(m.op==BN_EFFECT){
            if(!boss_net_ledger_accept(&r->deliveries,m.revision))return;
        }
        if(m.op==BN_REWARD){memcpy(r->reward,body,m.length);r->reward_size=m.length;r->reward_reserved=0;}
        if(m.owner!=gNetworkPlayerLocal->globalIndex||!m.revision)effect_apply(r,body,m.length,m.op==BN_REWARD);
    }
}
static void submit(BossRecord *r,uint8_t op,uint8_t owner,uint32_t revision,const void *body,uint16_t length,int reliable){
    if(gNetworkType==NT_SERVER){
        BossNetMessage m={r->key,r->lease.epoch,revision,gNetworkPlayerLocal->currLevelAreaSeqId,length,op,owner};
        accept_server(r,&m,body,gNetworkPlayerLocal->globalIndex,1);
    }else send_message(r,op,owner,revision,body,length,PACKET_DESTINATION_SERVER,reliable);
}
/* Called after the established native serializer, before its generic send. */
int boss_net_object_packet(struct Object *o,struct Packet *p){
    if(!boss_net_managed(o))return 0;
    BossRecord *r=for_object(o);
    if(!r||(!boss_net_simulates(o)&&r->pass_to==BOSS_NET_NO_OWNER))return 1;
    if(r->reward_reserved)return 1; // publish post-reward state only after server commit
    /* packet_init produced the ordinary 10-byte area header. */
    if(p->orderedGroupId||p->dataLength<10||p->dataLength-10>OBJECT_BODY_MAX)return 1;
    unsigned size=p->dataLength-10;
    if(!state_valid(r,p->buffer+10,size))return 1;
    uint8_t target=r->pass_to,op=target==BOSS_NET_NO_OWNER?BN_STATE:BN_PASS;
    uint32_t revision=++r->outgoing;
    uint8_t body[OBJECT_BODY_MAX+4];memcpy(body,p->buffer+10,size);
    if(op==BN_PASS){r->frozen=1;r->pass_to=BOSS_NET_NO_OWNER;
        for(int i=0;i<4;i++)body[size++]=(uint8_t)(r->event_out>>(8*i));}
    submit(r,op,target,revision,body,size,op==BN_PASS||p->reliable);
    return 1;
}
int boss_net_wrap_effect(struct Packet *p){
    if(!boss_net_enabled()||!p||
       (p->packetType!=PACKET_SPAWN_OBJECTS&&p->packetType!=PACKET_SPAWN_STAR))return 0;
    if(applying)return 1; // native star creation on receive must not echo a spawn
    struct Object *root=gCurrentObject;
    for(unsigned i=0;root&&i<4&&!boss_kind(root);i++)root=root->parentObj==root?NULL:root->parentObj;
    if(!boss_net_managed(root))return 0;
    /* Ordered native area baselines carry existing dynamic coins. They are
     * not new effects of whatever object last occupied gCurrentObject. */
    if(boss_kind(root)==4&&p->orderedGroupId)return 0;
    if(!boss_net_simulates(root))return 1;
    BossRecord *r=for_object(root);if(!r)return 1;
    int reward=p->packetType==PACKET_SPAWN_STAR;
    if(p->packetType==PACKET_SPAWN_OBJECTS&&p->dataLength>=24){
        uint32_t behavior;memcpy(&behavior,p->buffer+20,4);
        const BehaviorScript *b=get_behavior_from_id(behavior);
        reward=b==bhvBowserKey||b==bhvGrandStar;
    }
    if(reward){if(r->reward_reserved||r->reward_seen)return 1;r->reward_reserved=1;r->reward_seen=1;}
    if(p->orderedGroupId||p->dataLength>BOSS_NET_BODY)return 1;
    submit(r,reward?BN_REWARD:BN_EFFECT,r->lease.owner,++r->event_out,p->buffer,p->dataLength,1);
    return 1;
}
int boss_net_legacy_packet_valid(struct Packet *p){
    if(!boss_net_enabled()||p->packetType!=PACKET_OBJECT||applying)return 1;
    if(p->cursor+13>p->dataLength)return 0;
    uint32_t behavior;memcpy(&behavior,p->buffer+p->cursor+9,4);
    const BehaviorScript *b=get_behavior_from_id(behavior);
    return b!=bhvKingBobomb&&b!=bhvBowser&&b!=bhvWhompKingBoss&&b!=bhvSmallWhomp;
}
static int bowser_pending_defeat_dialog(struct Object *o){
    return boss_kind(o)==2&&o->oAction==4&&
        (o->oSubAction==3||o->oSubAction==10)&&!o->oDialogState&&o->oBowserUnkF8<2;
}
static int can_pass(struct Object *o){
    if(o->oHeldState!=HELD_FREE||o->oDialogState)return 0;
    for(int i=0;i<MAX_PLAYERS;i++)if(gMarioStates[i].heldObj==o||gMarioStates[i].heldByObj==o)return 0;
    if(boss_kind(o)>=3)return o->oAction==0||o->oAction==1||o->oAction==2||
        (boss_kind(o)==3&&o->oAction==8&&!o->oDialogState);
    if(boss_kind(o)==1)return (o->oAction==0&&o->oSubAction==0)||o->oAction==1||o->oAction==2||
        (o->oAction==5&&o->oSubAction==3)||o->oAction==7;
    return o->oAction==0||o->oAction==3||
        (o->oAction==4&&o->oSubAction==2)||bowser_pending_defeat_dialog(o);
}
static void record_status(struct Object *o,BossRecord *r){
#ifdef ROCKET_CAR_QA
    if(o->activeFlags!=ACTIVE_FLAG_DEACTIVATED&&r->frame+3>gGlobalTimer)return;
    r->frame=gGlobalTimer;
    unsigned rewards=0;
    for(int list=0;gObjectLists&&list<NUM_OBJ_LISTS;list++){
        struct ObjectNode *head=&gObjectLists[list];
        for(struct ObjectNode *node=head->next;node&&node!=head;node=node->next){
            struct Object *reward=(struct Object*)node;
            if((reward->activeFlags&ACTIVE_FLAG_ACTIVE)&&
               (reward->behavior==bhvBowserKey||reward->behavior==bhvGrandStar||reward->behavior==bhvStarSpawnCoordinates))rewards++;
        }
    }
    fprintf(stderr,"BOSS_NET_NATIVE {\"frame\":%u,\"global\":%u,\"owner\":%u,\"epoch\":%u,\"revision\":%u,\"level\":%u,\"kind\":%u,\"sync\":%u,\"active\":%u,\"health\":%d,\"action\":%d,\"sub\":%d,\"held\":%u,\"pos\":[%.4f,%.4f,%.4f],\"rewards\":%u,\"frozen\":%u,\"camera_cutscene\":%u,\"dialog\":%d,\"player_action\":%u}\n",
        gGlobalTimer,gNetworkPlayerLocal->globalIndex,r->lease.owner,r->lease.epoch,r->lease.revision,r->key.level,r->key.kind,r->key.sync,(unsigned)(u16)o->activeFlags,
        o->oHealth,o->oAction,o->oSubAction,o->oHeldState,o->oPosX,o->oPosY,o->oPosZ,rewards,r->frozen,
        gCamera?gCamera->cutscene:0,get_dialog_id(),gMarioState?gMarioState->action:0);
#else
    (void)o;(void)r;
#endif
}
int boss_net_begin(struct Object *o){
    if(!boss_net_managed(o))return 1;
    if(!gNetworkAreaLoaded||!gNetworkPlayerLocal->currAreaSyncValid||!o->oSyncID)return 0;
    BossNetKey key=object_key(o);BossRecord *r=find_record(&key,1);if(!r)return 0;
    bind_area(r);
    if(r->object!=o){
        r->object=o;r->applied_epoch=0;r->joined=-10;
    }
    double now=clock_elapsed_f64();
    if(now-r->joined>=1){r->joined=now;submit(r,BN_JOIN,0,0,NULL,0,1);}
    if(r->reward_size&&!r->reward_seen)effect_apply(r,r->reward,r->reward_size,1);
    if(!r->admitted||r->lease.owner!=gNetworkPlayerLocal->globalIndex||r->frozen){apply_state(r);record_status(o,r);return 0;}
    apply_state(r);
    if(!boss_net_simulates(o)){record_status(o,r);return 0;}
    if(can_pass(o)){
        struct MarioState *nearest=nearest_mario_state_to_object(o);
        if(nearest&&nearest->playerIndex&&nearest->playerIndex<MAX_PLAYERS){
            struct NetworkPlayer *np=&gNetworkPlayers[nearest->playerIndex];
            float other=dist_between_objects(o,nearest->marioObj);
            float own=gMarioStates[0].marioObj?dist_between_objects(o,gMarioStates[0].marioObj):100000;
            /* Native defeat waits advance at 700 units, before the normal
             * 200-unit ownership margin may be met. Only the simulator can
             * open the nearest player's local dialog, so permit that pending
             * dialog's recipient to take over. Keep ordinary hysteresis and
             * never transfer an active dialog or the subsequent reward phase. */
            int dialog_recipient=(bowser_pending_defeat_dialog(o)&&other<700)||
                (boss_kind(o)==3&&(o->oAction==0||o->oAction==8)&&other<600);
            if(np->connected&&(other+200<own||dialog_recipient)&&other<1800){
                r->pass_to=np->globalIndex;network_send_object_reliability(o,true);
                if(r->frozen)return 0;
            }
        }
    }
    return boss_net_simulates(o);
}
void boss_net_end(struct Object *o){
    if(boss_net_managed(o)&&boss_net_simulates(o)){
        network_send_object_reliability(o,o->activeFlags==ACTIVE_FLAG_DEACTIVATED);
        BossRecord *r=for_object(o);if(r)record_status(o,r);
    }
}
void boss_net_update(void){
    if(!boss_net_enabled())return;
    for(unsigned i=0;i<BOSS_ROOMS;i++){
        BossRecord *r=&records[i];if(!r->used)continue;
        bind_area(r);
        if(r->pending_coin_count&&gNetworkAreaLoaded&&gMarioStates[0].marioObj&&in_room(&r->key,gNetworkPlayerLocal)){
            unsigned count=r->pending_coin_count;r->pending_coin_count=0;
            for(unsigned coin=0;coin<count;coin++)effect_apply(r,r->pending_coins[coin],WHOMP_COIN_BODY,0);
        }
        if(gNetworkType!=NT_SERVER)continue;
        for(unsigned j=0;j<MAX_PLAYERS;j++)if(r->present[j]&&
            !in_room(&r->key,network_player_from_global_index(j)))r->present[j]=0;
        if(member(r,r->lease.owner))continue;
        // A loading peer already in the room keeps the native encounter alive
        // even before its first JOIN. Retire only a genuinely empty area.
        if(!any_occupant(r)){
            if(r->lease.epoch)clear_record(r);
            continue;
        }
        unsigned next=BOSS_NET_NO_OWNER;
        for(unsigned j=0;j<MAX_PLAYERS;j++)if(member(r,j)){next=j;break;}
        if(next!=r->lease.owner){
            /* The vanished participant cannot complete a native grab/dialog.
             * Keep damage and free-flight state, cancel only that interaction. */
            if(r->state_size>=13+4*OBJECT_NUM_FIELDS){
                struct Object v={0};memcpy(v.rawData.asU32,r->state+13,4*OBJECT_NUM_FIELDS);
                if(v.oHeldState!=HELD_FREE||(r->key.kind==1&&v.oAction==3)){
                    v.oHeldState=HELD_FREE;v.oAction=r->key.kind==1?2:0;v.oPrevAction=v.oAction;
                    v.oSubAction=v.oTimer=0;v.oForwardVel=v.oVelX=v.oVelY=v.oVelZ=0;
                    v.oInteractStatus=0;v.oIntangibleTimer=0;
                }
                if(v.oDialogState){
                    v.oDialogState=v.oDialogResponse=0;
                    if(r->key.kind==1&&v.oAction==0)v.oSubAction=0;
                    if(r->key.kind==1&&v.oAction==5)v.oSubAction=3;
                    if(r->key.kind==2&&v.oAction==4)v.oBowserUnkF8=0;
                }
                memcpy(r->state+13,v.rawData.asU32,4*OBJECT_NUM_FIELDS);
            }
            grant(r,next);
        }
    }
}
void boss_net_disconnected(unsigned global){
    if(global>=MAX_PLAYERS)return;
    for(unsigned i=0;i<BOSS_ROOMS;i++)if(records[i].used)records[i].present[global]=0;
    boss_net_update();
}
