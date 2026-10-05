/* Actual authority implementation with explicit inert transport/object hosts.
 * These tests exercise races and ingress, not native gameplay or rendering. */
#include "../../../src/pc/boss_net.c"
#include "sm64.h"
#include "game/hardcoded.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks,sends,applies,effects,effect_attempts;
static BossNetMessage transmitted[MAX_PLAYERS];
static bool transmitted_reliable[MAX_PLAYERS];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
struct CLIOptions gCLIOpts;
enum NetworkType gNetworkType;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal,*gNetworkPlayerServer;
struct MarioState gMarioStates[MAX_PLAYERS];
struct ServerSettings gServerSettings;
struct BehaviorValues gBehaviorValues;
struct Object *gCurrentObject;
bool gNetworkAreaLoaded;
s16 gCurrCourseNum,gCurrActStarNum,gCurrLevelNum,gCurrAreaIndex;
static struct Object boss,player,remote_player;
static struct SyncObject sync_object;
static int sync_available;
const BehaviorScript bhvKingBobomb[]={1},bhvBowser[]={2},bhvBowserKey[]={3},bhvGrandStar[]={4},bhvFlameMovingForwardGrowing[]={5},bhvBowserShockWave[]={6},bhvBlueBowserFlame[]={7};
const BehaviorScript bhvWhompKingBoss[]={8},bhvSmallWhomp[]={9},bhvSingleCoinGetsSpawned[]={10};
static double now=1;
const BehaviorScript *get_behavior_from_id(enum BehaviorId id){return id==1?bhvKingBobomb:id==2?bhvBowser:id==3?bhvBowserKey:id==4?bhvGrandStar:id==5?bhvFlameMovingForwardGrowing:id==6?bhvBowserShockWave:id==7?bhvBlueBowserFlame:id==8?bhvWhompKingBoss:id==9?bhvSmallWhomp:id==10?bhvSingleCoinGetsSpawned:NULL;}
f64 clock_elapsed_f64(void){return now;}
struct NetworkPlayer *network_player_from_global_index(u8 global){
    for(unsigned i=0;i<MAX_PLAYERS;i++)if(gNetworkPlayers[i].connected&&gNetworkPlayers[i].globalIndex==global)return &gNetworkPlayers[i];
    return NULL;
}
void network_send(struct Packet *p){(void)p;sends++;}
void network_send_to(u8 index,struct Packet *p){
    if(index==PACKET_DESTINATION_SERVER)index=gNetworkPlayerServer->localIndex;
    CHECK(index<MAX_PLAYERS);
    CHECK(boss_net_unpack(&transmitted[index],p->buffer+5,p->dataLength-5));transmitted_reliable[index]=p->reliable;sends++;
}
struct SyncObject *sync_object_get(u32 id){return sync_available&&id==boss.oSyncID?&sync_object:NULL;}
bool sync_object_is_initialized(u32 id){return sync_available&&id==boss.oSyncID;}
void network_receive_object(struct Packet *p){
    CHECK(boss_net_applying());CHECK(p->dataLength>=13+4*OBJECT_NUM_FIELDS+1);
    memcpy(boss.rawData.asU32,p->buffer+13,4*OBJECT_NUM_FIELDS);applies++;
    if(boss_kind(&boss)>=3)memcpy(&boss.activeFlags,p->buffer+14+4*OBJECT_NUM_FIELDS+2,2);
}
void packet_process(struct Packet *p){
    CHECK(boss_net_applying());effect_attempts++;
    /* Match the native readers' missing-parent early returns. These failures
     * do not set p.error and must not permanently consume a reward. */
    if(p->packetType==PACKET_SPAWN_STAR&&!gMarioStates[0].marioObj)return;
    if(p->packetType==PACKET_SPAWN_OBJECTS){
        u8 count,ctx;u32 parentId;
        packet_read(p,&count,sizeof count);packet_read(p,&ctx,sizeof ctx);
        packet_read(p,&parentId,sizeof parentId);CHECK(count&&ctx);
        if(parentId==UINT32_MAX){if(!gMarioStates[0].marioObj)return;}
        else{
            struct SyncObject *so=sync_object_get(parentId);
            if(!so)return;
            if(!so->o&&!gMarioStates[0].marioObj)return;
        }
    }
    effects++;
}
static unsigned state_body(uint8_t *body);
void network_send_object_reliability(struct Object *o,bool reliable){
    /* Native serialization is an explicit host stub; PASS/GRANT, freezing,
     * authentication and the event barrier use the actual production code. */
    struct Packet p={0};p.reliable=reliable;
    p.dataLength=10+state_body(p.buffer+10);
    CHECK(boss_net_object_packet(o,&p));
}

static BossRecord *fresh(void){
    boss_net_reset();sends=applies=effects=effect_attempts=0;sync_available=1;
    memset(&boss,0,sizeof boss);memset(&sync_object,0,sizeof sync_object);
    memset(&player,0,sizeof player);memset(&remote_player,0,sizeof remote_player);
    memset(gNetworkPlayers,0,sizeof gNetworkPlayers);memset(gMarioStates,0,sizeof gMarioStates);
    gMarioStates[0].marioObj=&player;
    gCLIOpts.characterNet=true;gNetworkType=NT_SERVER;gNetworkAreaLoaded=true;
    gCurrCourseNum=1;gCurrActStarNum=1;gCurrLevelNum=9;gCurrAreaIndex=1;
    for(unsigned i=0;i<3;i++){
        struct NetworkPlayer *np=&gNetworkPlayers[i];np->connected=true;np->localIndex=np->globalIndex=i;
        np->currCourseNum=1;np->currActNum=1;np->currLevelNum=9;np->currAreaIndex=1;
        np->currLevelAreaSeqId=7;np->currAreaSyncValid=true;
    }
    gNetworkPlayerLocal=gNetworkPlayerServer=&gNetworkPlayers[0];
    boss.behavior=bhvKingBobomb;boss.oSyncID=10;boss.oHealth=3;boss.oAction=2;boss.oPosY=4500;
    boss.activeFlags=ACTIVE_FLAG_ACTIVE;
    sync_object.id=10;sync_object.o=&boss;sync_object.fullObjectSync=true;
    BossNetKey key=object_key(&boss);BossRecord *r=find_record(&key,1);
    r->object=&boss;r->local_token=7;
    for(unsigned i=0;i<3;i++){r->present[i]=1;r->members[i]=7;}
    grant(r,1);return r;
}
static unsigned state_body(uint8_t *body){
    memset(body,0,OBJECT_BODY_MAX);body[0]=1;uint32_t sync=10,behavior=boss_kind(&boss);
    if(behavior>=3)behavior+=5;
    memcpy(body+1,&sync,4);memcpy(body+9,&behavior,4);memcpy(body+13,boss.rawData.asU32,4*OBJECT_NUM_FIELDS);
    if(boss_kind(&boss)>=3){
        unsigned at=13+4*OBJECT_NUM_FIELDS;body[at]=8;uint8_t *extra=body+at+1;
        memcpy(extra+2,&boss.activeFlags,2);
        float scale=1;for(int i=0;i<3;i++)memcpy(extra+4+4*i,&scale,4);
        memcpy(extra+16,&boss.oAngleVelPitch,4);memcpy(extra+20,&boss.oFaceAnglePitch,4);
        memcpy(extra+24,&boss.oForwardVel,4);memcpy(extra+28,&boss.oHealth,4);memcpy(extra+32,&boss.oFaceAnglePitch,4);
        sync_object.extraFieldCount=8;
        const u8 sizes[]={2,2,12,4,4,4,4,4};memcpy(sync_object.extraFieldsSizeBytes,sizes,sizeof sizes);
        return at+1+36;
    }
    return 13+4*OBJECT_NUM_FIELDS+1;
}
static BossNetMessage message(BossRecord *r,unsigned op,unsigned rev,unsigned length){
    BossNetMessage m={r->key,r->lease.epoch,rev,7,length,op,r->lease.owner};return m;
}
static void ingress(BossNetMessage *m,const void *body,unsigned sender){
    struct Packet p={0};uint8_t wire[BOSS_NET_HEADER+BOSS_NET_BODY];
    int size=boss_net_pack(wire,sizeof wire,m,body);CHECK(size>0);
    packet_init(&p,PACKET_BOSS_STATE,true,PLMT_NONE);packet_write(&p,wire,size);
    p.localIndex=sender;p.cursor=5;p.destGlobalId=gNetworkType==NT_CLIENT?gNetworkPlayerLocal->globalIndex:0;boss_net_receive(&p);
}
static BossRecord *fresh_client(const BehaviorScript *behavior){
    fresh();boss_net_reset();boss.behavior=behavior;gNetworkType=NT_CLIENT;
    gNetworkPlayers[0].globalIndex=2;gNetworkPlayers[1].globalIndex=0;gNetworkPlayers[2].globalIndex=1;
    gNetworkPlayerServer=&gNetworkPlayers[1];
    BossNetKey key=object_key(&boss);return find_record(&key,1);
}
static BossRecord *fresh_defeated_bowser(void){
    fresh();boss_net_reset();boss.behavior=bhvBowser;boss.oHealth=0;
    boss.oAction=4;boss.oSubAction=2;boss.oPosY=307;
    player.oPosY=remote_player.oPosY=307;
    player.oPosZ=780;remote_player.oPosZ=650;
    for(unsigned i=0;i<2;i++){
        gMarioStates[i].marioObj=i?&remote_player:&player;
        gMarioStates[i].playerIndex=i;gMarioStates[i].visibleToObjects=true;
        gMarioStates[i].action=ACT_IDLE;
    }
    BossNetKey key=object_key(&boss);BossRecord *r=find_record(&key,1);
    r->object=&boss;r->local_token=7;
    for(unsigned i=0;i<3;i++){r->present[i]=1;r->members[i]=7;}
    grant(r,0);apply_state(r);return r;
}
static void defeat_dialog_handoff_tests(void){
    /* r5 geometry: the remote reaches native Bowser's 700-unit dialog gate
     * before it is 200 units nearer than the parked owner. Real native nearest
     * player selection and distances are linked, including visibility/area. */
    for(unsigned sub=3;sub<=10;sub+=7){
        BossRecord *r=fresh_defeated_bowser();uint32_t epoch=r->lease.epoch;
        CHECK(nearest_mario_state_to_object(&boss)==&gMarioStates[1]);
        CHECK(dist_between_objects(&boss,&remote_player)==650);
        CHECK(dist_between_objects(&boss,&player)==780);
        CHECK(boss_net_begin(&boss)&&r->lease.owner==0); // Ordinary sub2 hysteresis.
        boss.oSubAction=sub;boss.oBowserUnkF8=1; // Native wait gate has advanced.
        CHECK(!boss_net_begin(&boss));
        CHECK(r->lease.owner==1&&boss_net_newer(r->lease.epoch,epoch));
        CHECK(boss.oAction==4&&boss.oSubAction==(s32)sub&&boss.oBowserUnkF8==1);
        CHECK(!boss_net_simulates(&boss)&&!boss_net_begin(&boss));
        CHECK(!boss_net_begin(&boss)&&r->lease.owner==1); // No follower oscillation.

        // The new simulator's local player is nearest: no handback at the
        // next begin. Once a real dialog starts, even a closer peer cannot
        // steal it. Preserve the owner through the native reward phase.
        gNetworkType=NT_CLIENT;gNetworkPlayers[0].globalIndex=1;
        gNetworkPlayers[1].globalIndex=0;gNetworkPlayerServer=&gNetworkPlayers[1];
        gMarioStates[0].marioObj=&remote_player;gMarioStates[1].marioObj=&player;
        CHECK(boss_net_begin(&boss)&&r->lease.owner==1&&!r->frozen);
        for(unsigned dialog=1;dialog<=4;dialog++){
            boss.oDialogState=dialog;player.oPosZ=100;
            CHECK(boss_net_begin(&boss)&&r->lease.owner==1&&!r->frozen);
        }
        boss.oDialogState=0;boss.oBowserUnkF8=2;
        CHECK(boss_net_begin(&boss)&&r->lease.owner==1&&!r->frozen);
    }

    BossRecord *r=fresh_defeated_bowser();boss.oSubAction=3;
    gMarioStates[1].visibleToObjects=false;
    CHECK(boss_net_begin(&boss)&&r->lease.owner==0);
    gMarioStates[1].visibleToObjects=true;gMarioStates[1].action=ACT_BUBBLED;
    CHECK(boss_net_begin(&boss)&&r->lease.owner==0);
    gMarioStates[1].action=ACT_IDLE;gNetworkPlayers[1].currAreaIndex=2;
    CHECK(boss_net_begin(&boss)&&r->lease.owner==0);
    gNetworkPlayers[1].currAreaIndex=1;gNetworkPlayers[1].connected=false;
    CHECK(boss_net_begin(&boss)&&r->lease.owner==0);
    gNetworkPlayers[1].connected=true;remote_player.oPosZ=700;
    CHECK(boss_net_begin(&boss)&&r->lease.owner==0); // Strict native radius.
    remote_player.oPosZ=650;boss.oHeldState=HELD_HELD;
    CHECK(boss_net_begin(&boss)&&r->lease.owner==0);
    boss.oHeldState=HELD_FREE;gMarioStates[0].heldObj=&boss;
    CHECK(boss_net_begin(&boss)&&r->lease.owner==0);
    gMarioStates[0].heldObj=NULL;

    // An outstanding effect delays GRANT. The old owner must freeze after
    // PASS and cannot run a dialog meanwhile; repeated begins cannot resend.
    r->event_out=1;uint32_t epoch=r->lease.epoch;
    CHECK(!boss_net_begin(&boss)&&r->frozen&&r->pending_owner==1);
    unsigned sent=sends;
    CHECK(!boss_net_begin(&boss)&&sends==sent&&r->lease.epoch==epoch);
    uint8_t reward[11+31+4*OBJECT_NUM_FIELDS]={PACKET_SPAWN_OBJECTS,0,0,1,255,1,1,9,0,1,1};
    reward[11]=1;reward[12]=10;reward[20]=3;reward[24]=1;
    reward[sizeof reward-3]=255;reward[sizeof reward-2]=reward[sizeof reward-1]=255;
    BossNetMessage m=message(r,BN_REWARD,1,sizeof reward);ingress(&m,reward,0);
    CHECK(r->lease.owner==1&&!r->frozen&&effects==1);
    CHECK(r->reward_size==sizeof reward&&!boss_net_reward_available(&boss));
    ingress(&m,reward,0);CHECK(effects==1); // Late old-owner reward replay.
    m=message(r,BN_REWARD,1,sizeof reward);ingress(&m,reward,1);
    CHECK(effects==1&&!boss_net_reward_available(&boss));
    gNetworkPlayers[1].connected=false;boss_net_disconnected(1);apply_state(r);
    CHECK(r->lease.owner==0&&boss.oHealth==0&&boss.oSubAction==3);
    CHECK(!boss_net_reward_available(&boss)&&effects==1);
}
static void deferred_reward_tests(void){
    uint8_t reward[11+31+4*OBJECT_NUM_FIELDS]={PACKET_SPAWN_OBJECTS,0,0,1,255,1,1,9,0,1,1};
    reward[11]=1;reward[12]=10;reward[24]=1;
    reward[sizeof reward-3]=255;reward[sizeof reward-2]=reward[sizeof reward-1]=255;
    for(unsigned behavior=3;behavior<=4;behavior++){
        BossRecord *r=fresh_client(bhvBowser);reward[20]=behavior;sync_available=0;
        BossNetMessage m=message(r,BN_REWARD,0,sizeof reward);m.epoch=5;m.owner=1;
        ingress(&m,reward,1);ingress(&m,reward,1);
        CHECK(r->reward_size==sizeof reward&&!r->reward_seen&&!boss_net_reward_available(&boss));
        CHECK(!effects&&!effect_attempts);
        CHECK(!boss_net_begin(&boss));CHECK(!r->reward_seen&&!effect_attempts);
        sync_available=1;sync_object.o=NULL;
        CHECK(!boss_net_begin(&boss));CHECK(!r->reward_seen&&!effect_attempts);
        sync_object.o=&boss;
        CHECK(!boss_net_begin(&boss));CHECK(r->reward_seen&&effects==1&&effect_attempts==1);
        CHECK(!boss_net_begin(&boss));ingress(&m,reward,1);
        CHECK(effects==1&&effect_attempts==1); // Loop retry and reliable replay cannot duplicate.

        // A new area instance discards the old receipt, but delayed packets
        // for the previous token cannot create or consume its pending reward.
        gNetworkPlayerLocal->currLevelAreaSeqId=8;sync_available=0;
        CHECK(!boss_net_begin(&boss));ingress(&m,reward,1);
        CHECK(!r->reward_size&&!r->reward_seen&&effects==1);
        m.token=8;m.epoch=6;ingress(&m,reward,1);
        CHECK(r->reward_size&&!r->reward_seen&&effects==1);
        sync_available=1;CHECK(!boss_net_begin(&boss));ingress(&m,reward,1);
        CHECK(r->reward_seen&&effects==2&&effect_attempts==2);
    }
    BossRecord *r=fresh_client(bhvKingBobomb);
    uint8_t star[52]={PACKET_SPAWN_STAR,0,0,1,255,1,1,9,0,1};star[27]=255;
    BossNetMessage m=message(r,BN_REWARD,0,sizeof star);m.epoch=7;m.owner=1;
    gMarioStates[0].marioObj=NULL;ingress(&m,star,1);
    CHECK(r->reward_size&&!r->reward_seen&&!effects&&!effect_attempts);
    CHECK(!boss_net_begin(&boss));CHECK(!r->reward_seen&&!effect_attempts);
    gMarioStates[0].marioObj=&player;
    CHECK(!boss_net_begin(&boss));ingress(&m,star,1);
    CHECK(r->reward_seen&&effects==1&&effect_attempts==1);
}
static void whomp_authority_tests(void){
    const BehaviorScript *kinds[]={bhvWhompKingBoss,bhvSmallWhomp};
    for(unsigned kind=0;kind<2;kind++){
        fresh();boss_net_reset();boss.behavior=kinds[kind];boss.oAction=6;boss.oSubAction=0;
        BossNetKey key=object_key(&boss);BossRecord *r=find_record(&key,1);
        r->object=&boss;r->local_token=7;
        for(unsigned i=0;i<3;i++){r->present[i]=1;r->members[i]=7;}
        grant(r,1);CHECK(r->key.kind==3+kind&&boss_net_managed(&boss)&&!boss_net_simulates(&boss));
        uint8_t body[OBJECT_BODY_MAX+4],older[OBJECT_BODY_MAX+4];
        unsigned size=state_body(body);memcpy(older,body,size);
        unsigned extra=14+4*OBJECT_NUM_FIELDS;
        uint8_t malformed[OBJECT_BODY_MAX];memcpy(malformed,body,size);
        malformed[extra-1]=7;CHECK(!state_valid(r,malformed,size));
        memcpy(malformed,body,size);float nan=NAN;memcpy(malformed+extra+4,&nan,4);CHECK(!state_valid(r,malformed,size));
        memcpy(malformed,body,size);malformed[extra+28]++;CHECK(!state_valid(r,malformed,size));
        BossNetMessage m=message(r,BN_STATE,1,size);ingress(&m,body,1);
        CHECK(r->lease.revision==1&&boss.oHealth==3);
        for(unsigned revision=2;revision<=4;revision++){
            boss.oHealth=4-revision;boss.oSubAction=1;boss.oAction=revision==4?8:6;
            boss.activeFlags=kind&&revision==4?ACTIVE_FLAG_DEACTIVATED:ACTIVE_FLAG_ACTIVE;
            state_body(body);boss.oHealth=99;ingress(&m,older,2); // Wrong sender cannot restore.
            CHECK(boss.oHealth==99);
            m.revision=revision;ingress(&m,body,1);CHECK(boss.oHealth==(s32)(4-revision));
            CHECK(transmitted_reliable[2]); // Committed Whomp consequences are reliably forwarded.
            unsigned applied=applies;ingress(&m,older,1);CHECK(applies==applied&&boss.oHealth==(s32)(4-revision));
        }
        if(kind)CHECK(boss.activeFlags==ACTIVE_FLAG_DEACTIVATED&&transmitted_reliable[2]);
        CHECK(boss.oAction==8);uint32_t epoch=r->lease.epoch;
        gNetworkPlayers[1].connected=false;boss_net_disconnected(1);apply_state(r);
        CHECK(r->lease.owner==0&&r->lease.epoch!=epoch&&boss.oHealth==0&&boss.oAction==8);
        gNetworkPlayers[1].connected=true;ingress(&m,older,1);CHECK(boss.oHealth==0);
        CHECK(!can_pass(&boss)||!kind); // Small death phase never hands off.
        boss.oAction=6;boss.oSubAction=0;CHECK(!can_pass(&boss)); // Vulnerable window stays on one simulator.
        boss.oAction=2;CHECK(can_pass(&boss));
        struct Packet legacy={0};legacy.packetType=PACKET_OBJECT;legacy.dataLength=size;
        memcpy(legacy.buffer,body,size);CHECK(!boss_net_legacy_packet_valid(&legacy));
    }
    // Client follower applies server-authorized staged health and a canonical
    // star once. Old state/reward traffic is rejected across a reconnect token.
    BossRecord *r=fresh_client(bhvWhompKingBoss);
    uint8_t body[OBJECT_BODY_MAX+4],star[52]={PACKET_SPAWN_STAR,0,0,1,255,1,1,9,0,1};star[27]=255;
    boss.oHealth=2;boss.oAction=6;boss.oSubAction=1;unsigned size=state_body(body);memset(body+size,0,4);boss.oHealth=3;
    BossNetMessage m=message(r,BN_GRANT,1,size+4);m.epoch=8;m.owner=1;
    ingress(&m,body,1);CHECK(boss.oHealth==2&&!boss_net_simulates(&boss));
    boss.oHealth=0;boss.oAction=8;state_body(body);boss.oHealth=2;
    m=message(r,BN_STATE,2,size);ingress(&m,body,2);CHECK(boss.oHealth==2); // Not physical server.
    ingress(&m,body,1);CHECK(boss.oHealth==0&&boss.oAction==8);
    m=message(r,BN_REWARD,0,sizeof star);ingress(&m,star,1);ingress(&m,star,1);
    CHECK(effects==1&&!boss_net_reward_available(&boss));
    gNetworkPlayerLocal->currLevelAreaSeqId=8;CHECK(!boss_net_begin(&boss));
    ingress(&m,star,1);CHECK(effects==1&&!r->reward_seen);
    m.token=8;m.epoch=9;ingress(&m,star,1);ingress(&m,star,1);CHECK(effects==2);
    // Ordinary Whomp loot is an effect with a dynamic native coin identity,
    // not a replayable canonical boss reward. Surviving coins use area sync.
    r=fresh_client(bhvSmallWhomp);
    boss.oAction=8;boss.activeFlags=ACTIVE_FLAG_DEACTIVATED;size=state_body(body);memset(body+size,0,4);
    boss.activeFlags=ACTIVE_FLAG_ACTIVE;m=message(r,BN_GRANT,1,size+4);m.epoch=10;m.owner=1;
    ingress(&m,body,1);CHECK(boss.activeFlags==ACTIVE_FLAG_DEACTIVATED&&!boss_net_simulates(&boss));
    uint8_t coin[11+31+4*OBJECT_NUM_FIELDS]={PACKET_SPAWN_OBJECTS,0,0,1,255,1,1,9,0,1,1};
    coin[11]=1;coin[12]=10;coin[16]=MODEL_YELLOW_COIN;coin[20]=10;
    struct Object value={0};value.oSyncID=8193;memcpy(coin+26,value.rawData.asU32,4*OBJECT_NUM_FIELDS);
    coin[sizeof coin-3]=255;coin[sizeof coin-2]=coin[sizeof coin-1]=255;
    m=message(r,BN_EFFECT,1,sizeof coin);m.epoch=10;m.owner=1;
    ingress(&m,coin,1);ingress(&m,coin,1);CHECK(effects==1&&!r->reward_size);
    m.revision=2;coin[20]=3;ingress(&m,coin,1);CHECK(effects==1); // No key as Whomp loot.
    coin[20]=10;coin[12]=11;ingress(&m,coin,1);CHECK(effects==1); // Wrong parent.
    coin[12]=10;m.op=BN_REWARD;ingress(&m,coin,1);CHECK(effects==1&&!r->reward_size);
    // Reordered deletion may have forgotten the parent before reliable loot.
    sync_available=0;m.op=BN_EFFECT;m.revision=2;ingress(&m,coin,1);
    CHECK(effects==2);sync_available=1;
    // Current-entry deliveries wait through loading, then apply exactly once.
    gNetworkAreaLoaded=false;
    for(unsigned revision=3;revision<=12;revision++){m.revision=revision;ingress(&m,coin,1);ingress(&m,coin,1);}
    CHECK(effects==2&&r->pending_coin_count==10);
    gNetworkAreaLoaded=true;boss_net_update();boss_net_update();CHECK(effects==12&&!r->pending_coin_count);
    gNetworkAreaLoaded=false;m.revision=13;ingress(&m,coin,1);CHECK(r->pending_coin_count==1);
    gNetworkAreaLoaded=true;
    // A reconnect baseline skips old coin effects, preventing free loot.
    gNetworkPlayerLocal->currLevelAreaSeqId=8;CHECK(!boss_net_begin(&boss));
    CHECK(!r->pending_coin_count);
    uint8_t baseline[4]={13,0,0,0};m=message(r,BN_GRANT,1,4);m.token=8;m.epoch=11;m.owner=1;
    ingress(&m,baseline,1);m=message(r,BN_EFFECT,1,sizeof coin);m.token=8;ingress(&m,coin,1);
    CHECK(effects==12&&r->deliveries.contiguous==13);
}
int main(void){
    uint8_t body[OBJECT_BODY_MAX+4];BossRecord *r=fresh();unsigned size=state_body(body);
    BossNetMessage m=message(r,BN_STATE,1,size);ingress(&m,body,1);
    CHECK(r->lease.revision==1&&r->state_size==size&&boss.oHealth==3);
    unsigned old_applies=applies;
    boss.oHealth=0;state_body(body);m.revision=2;ingress(&m,body,2);
    CHECK(r->lease.revision==1&&applies==old_applies); // wrong physical owner
    m.revision=1;ingress(&m,body,1);CHECK(r->lease.revision==1&&applies==old_applies);
    m.revision=2;m.epoch++;ingress(&m,body,1);CHECK(r->lease.revision==1);
    m.epoch--;m.token++;ingress(&m,body,1);CHECK(r->lease.revision==1);

    /* PASS final state can arrive before either earlier reliable effect. */
    boss.oHealth=2;boss.oTimer=50;size=state_body(body);body[size]=2;body[size+1]=body[size+2]=body[size+3]=0;
    m=message(r,BN_PASS,2,size+4);m.owner=2;uint32_t old_epoch=m.epoch;ingress(&m,body,1);
    CHECK(r->lease.owner==1&&r->pending_owner==2&&r->lease.revision==2);
    uint8_t effect[52]={PACKET_SPAWN_STAR,0,0,1,255,1,1,9,0,1};effect[27]=255;
    m=message(r,BN_REWARD,2,sizeof effect);ingress(&m,effect,1);
    CHECK(r->lease.owner==1&&r->lease.event==0);
    m.revision=1;ingress(&m,effect,1);
    CHECK(r->lease.owner==2&&r->lease.epoch!=old_epoch&&boss.oHealth==2&&effects==1);
    unsigned before=sends;m=message(r,BN_STATE,100,size);m.epoch=old_epoch;ingress(&m,body,1);
    CHECK(sends==before&&r->lease.owner==2&&boss.oHealth==2);
    CHECK(!boss_net_player_holds(&boss,1)&&!boss_net_player_holds(&boss,2));
    boss.oHeldState=HELD_HELD;CHECK(!boss_net_player_holds(&boss,1)&&boss_net_player_holds(&boss,2));

    /* Disconnect cancels the grab, preserving the canonical remaining health. */
    boss.oHealth=1;size=state_body(body);m=message(r,BN_STATE,1,size);ingress(&m,body,2);
    gNetworkPlayers[2].connected=false;boss_net_disconnected(2);
    CHECK(r->lease.owner==0&&boss.oHealth==1);
    apply_state(r);CHECK(boss.oHeldState==HELD_FREE&&boss.oAction==2&&boss.oHealth==1);
    CHECK(!boss_net_player_holds(&boss,2));
    /* A new connection in the same global slot cannot reuse its prior lease. */
    gNetworkPlayers[2].connected=true;ingress(&m,body,2);CHECK(r->lease.owner==0);

    /* Applying an owner's own cached previous step must not rewind its timer. */
    r->applied_epoch=r->lease.epoch;boss.oTimer=51;apply_state(r);CHECK(boss.oTimer==51);
    CHECK(boss_net_simulates(&boss));r->frozen=1;CHECK(!boss_net_simulates(&boss));r->frozen=0;

    /* Reward identity is a boss-record property, retained across lease grants. */
    r=fresh();m=message(r,BN_REWARD,1,sizeof effect);ingress(&m,effect,1);
    CHECK(r->reward_size==sizeof effect&&effects==1);ingress(&m,effect,1);CHECK(effects==1);
    grant(r,2);CHECK(r->reward_size==sizeof effect&&!boss_net_reward_available(&boss));
    m=message(r,BN_REWARD,1,sizeof effect);ingress(&m,effect,2);CHECK(effects==1);

    /* Invalid nested payloads never advance the event barrier or reach the
     * ordinary native spawn parser, even on a server outside the area. */
    r=fresh();gNetworkPlayerLocal->currLevelNum=16;
    m=message(r,BN_REWARD,1,10);ingress(&m,effect,1);
    CHECK(r->source_events.highest==0&&!r->reward_size&&effects==0);
    m.length=sizeof effect;uint8_t invalid[52];memcpy(invalid,effect,sizeof invalid);
    invalid[13]=128;invalid[14]=127;ingress(&m,invalid,1); // NaN world coordinate
    CHECK(r->source_events.highest==0&&!r->reward_size);
    ingress(&m,effect,1);CHECK(r->reward_size==sizeof effect&&effects==0);
    gNetworkPlayerLocal->currLevelNum=9;

    /* Already committed effects remain applicable after a newer grant. */
    r=fresh();boss_net_reset();boss.behavior=bhvBowser;gNetworkType=NT_CLIENT;
    gNetworkPlayers[0].globalIndex=2;gNetworkPlayers[1].globalIndex=0;gNetworkPlayers[2].globalIndex=1;
    gNetworkPlayerServer=&gNetworkPlayers[1];
    BossNetKey key=object_key(&boss);r=find_record(&key,1);
    uint8_t baseline[4]={0};m=message(r,BN_GRANT,0,sizeof baseline);m.epoch=2;m.owner=2;
    ingress(&m,baseline,1);CHECK(r->lease.epoch==2&&r->deliveries_ready);
    uint8_t flame[11+31+4*OBJECT_NUM_FIELDS]={PACKET_SPAWN_OBJECTS,0,0,1,255,1,1,9,0,1,1};
    flame[11]=1;flame[12]=10;flame[20]=5;flame[24]=1;
    flame[sizeof flame-3]=255;flame[sizeof flame-2]=flame[sizeof flame-1]=255;
    m=message(r,BN_EFFECT,100,sizeof flame);m.owner=1;m.epoch=1;ingress(&m,flame,1);
    CHECK(effects==1&&r->deliveries.highest==100&&r->deliveries.contiguous==0);
    for(unsigned event=1;event<100;event++){m.revision=event;ingress(&m,flame,1);}
    CHECK(effects==100&&r->deliveries.contiguous==100);
    ingress(&m,flame,1);CHECK(effects==100);
    // Neither an invalid count nor a forged physical sender changes the ledger.
    m.revision=101;flame[10]=9;ingress(&m,flame,1);CHECK(effects==100&&r->deliveries.highest==100);
    flame[10]=1;ingress(&m,flame,2);CHECK(effects==100);

    /* A newly joined client can receive an effect before its first GRANT.
     * The baseline skips history without forgetting that later receipt. */
    boss_net_reset();effects=0;r=find_record(&key,1);
    m=message(r,BN_EFFECT,105,sizeof flame);m.epoch=4;m.owner=1;
    ingress(&m,flame,1);CHECK(effects==1&&!r->deliveries_ready);
    baseline[0]=100;m=message(r,BN_GRANT,0,sizeof baseline);m.epoch=4;m.owner=1;
    ingress(&m,baseline,1);CHECK(r->admitted&&r->deliveries_ready&&r->deliveries.contiguous==100);
    m=message(r,BN_EFFECT,105,sizeof flame);ingress(&m,flame,1);CHECK(effects==1);
    for(unsigned i=101;i<105;i++){m.revision=i;ingress(&m,flame,1);}
    CHECK(effects==5&&r->deliveries.contiguous==105);

    /* Reward ingress can precede the first native object loop. Its canonical
     * replay must not spawn another reward when that loop binds the object. */
    boss_net_reset();effects=0;boss.behavior=bhvKingBobomb;key=object_key(&boss);r=find_record(&key,1);
    m=message(r,BN_REWARD,0,sizeof effect);m.epoch=5;m.owner=1;
    ingress(&m,effect,1);CHECK(effects==1&&r->reward_seen);
    CHECK(!boss_net_begin(&boss));CHECK(r->reward_seen&&!r->admitted);
    baseline[0]=0;m=message(r,BN_GRANT,0,sizeof baseline);m.epoch=5;m.owner=1;
    ingress(&m,baseline,1);CHECK(r->admitted);
    m=message(r,BN_REWARD,0,sizeof effect);ingress(&m,effect,1);CHECK(effects==1);

    // A canonical reward received before native area loading is held until
    // the object loop is ready, and already forbids another authored spawn.
    boss_net_reset();effects=0;r=find_record(&key,1);gNetworkAreaLoaded=false;
    m=message(r,BN_REWARD,0,sizeof effect);m.epoch=5;m.owner=1;ingress(&m,effect,1);
    CHECK(effects==0&&r->reward_size&&!boss_net_reward_available(&boss));
    gNetworkAreaLoaded=true;CHECK(!boss_net_begin(&boss));CHECK(effects==1&&r->reward_seen);
    m=message(r,BN_GRANT,0,sizeof baseline);m.epoch=5;m.owner=1;ingress(&m,baseline,1);

    /* Fresh area entry cannot consume an old defeated/hidden presentation,
     * nor delayed state/effect/reward/GRANT packets for the prior entry. */
    boss.oHealth=0;size=state_body(body);m=message(r,BN_STATE,1,size);ingress(&m,body,1);
    CHECK(r->state_size==size);unsigned saved_applies=applies;
    boss.oHealth=3;gNetworkPlayerLocal->currLevelAreaSeqId=8;
    CHECK(!boss_net_begin(&boss));CHECK(boss.oHealth==3&&applies==saved_applies&&!r->state_size);
    m=message(r,BN_GRANT,1,size+4);m.epoch=5;m.owner=1;
    memset(body+size,0,4);ingress(&m,body,1);CHECK(!r->admitted&&boss.oHealth==3);
    m=message(r,BN_REWARD,0,sizeof effect);m.epoch=5;ingress(&m,effect,1);CHECK(effects==1&&!r->reward_seen);
    m=message(r,BN_STATE,2,size);m.epoch=5;ingress(&m,body,1);CHECK(!r->state_size);
    m=message(r,BN_GRANT,0,sizeof baseline);m.epoch=6;m.owner=2;m.token=8;
    ingress(&m,baseline,1);CHECK(r->admitted&&boss.oHealth==3&&r->lease.owner==2);
    CHECK(boss_net_begin(&boss)&&boss_net_simulates(&boss));

    /* Ownership admission is insufficient while native child fields are
     * still registering: default local health must never be published. */
    boss_net_reset();r=find_record(&key,1);boss.oHealth=1;size=state_body(body);
    memset(body+size,0,4);boss.oHealth=3;sync_object.extraFieldCount=1;
    m=message(r,BN_GRANT,10,size+4);m.epoch=7;m.owner=2;m.token=8;
    ingress(&m,body,1);CHECK(r->admitted&&!boss_net_simulates(&boss)&&boss.oHealth==3);
    CHECK(!boss_net_begin(&boss)&&r->applied_epoch!=r->lease.epoch&&boss.oHealth==3);
    sync_object.extraFieldCount=0;
    CHECK(boss_net_begin(&boss)&&boss_net_simulates(&boss)&&boss.oHealth==1);

    /* During loading, STATE can outrun an older reliable GRANT. Retrying the
     * latter must apply the newest cache, never lower its revision. */
    boss_net_reset();r=find_record(&key,1);boss.oHealth=2;size=state_body(body);
    memset(body+size,0,4);uint8_t older[OBJECT_BODY_MAX+4];memcpy(older,body,size+4);
    boss.oHealth=3;sync_object.extraFieldCount=1;
    m=message(r,BN_GRANT,10,size+4);m.epoch=8;m.owner=1;m.token=8;ingress(&m,older,1);
    boss.oHealth=1;state_body(body);boss.oHealth=3;
    m=message(r,BN_STATE,20,size);m.token=8;ingress(&m,body,1);
    CHECK(r->lease.revision==20&&boss.oHealth==3);
    m=message(r,BN_GRANT,10,size+4);m.token=8;ingress(&m,older,1);
    CHECK(r->lease.revision==20&&boss.oHealth==3);
    sync_object.extraFieldCount=0;ingress(&m,older,1);
    CHECK(r->lease.revision==20&&boss.oHealth==1&&r->applied_epoch==8);

    /* Loading occupants preserve canonical damage even before JOIN. Once
     * everyone actually leaves, a fresh entry starts a fresh native boss. */
    r=fresh();boss.oHealth=1;size=state_body(body);m=message(r,BN_STATE,1,size);ingress(&m,body,1);
    m=message(r,BN_REWARD,1,sizeof effect);ingress(&m,effect,1);
    old_epoch=r->lease.epoch;gNetworkPlayers[0].currLevelNum=16;gNetworkPlayers[1].currLevelNum=16;
    r->present[2]=0;boss_net_update();
    CHECK(r->lease.owner==BOSS_NET_NO_OWNER&&r->state_size==size&&r->reward_size==sizeof effect);
    m=message(r,BN_JOIN,0,0);ingress(&m,NULL,2);
    CHECK(r->lease.owner==2&&r->state_size==size&&r->reward_size==sizeof effect);
    gNetworkPlayers[2].currLevelNum=16;boss_net_update();
    CHECK(!r->state_size&&!r->reward_size&&!r->lease.epoch&&!r->source_events.bits);
    gNetworkPlayers[1].currLevelNum=9;gNetworkPlayers[1].currLevelAreaSeqId=8;
    m=message(r,BN_JOIN,0,0);m.token=8;ingress(&m,NULL,1);
    CHECK(r->lease.owner==1&&boss_net_newer(r->lease.epoch,old_epoch)&&!r->state_size&&!r->reward_size);
    CHECK(transmitted[1].op==BN_GRANT&&transmitted[1].token==8);
    boss.oHealth=3;size=state_body(body);m=message(r,BN_STATE,1,size);m.token=8;
    ingress(&m,body,1);CHECK(transmitted[1].op==BN_STATE&&transmitted[1].token==8);
    m=message(r,BN_REWARD,1,sizeof effect);m.token=8;ingress(&m,effect,1);
    CHECK(transmitted[1].op==BN_REWARD&&transmitted[1].token==8);

    /* Unload can retain the same location: the incremented area token plus
     * sync=false retires the last joined occupant before its reload JOIN. */
    r=fresh();boss.oHealth=1;size=state_body(body);m=message(r,BN_STATE,1,size);ingress(&m,body,1);
    gNetworkPlayers[0].currLevelNum=gNetworkPlayers[2].currLevelNum=16;
    gNetworkPlayers[1].currAreaSyncValid=false;gNetworkPlayers[1].currLevelAreaSeqId++;
    boss_net_update();CHECK(!r->lease.epoch&&!r->state_size&&!r->present[1]);
    gNetworkPlayers[1].currAreaSyncValid=true;m=message(r,BN_JOIN,0,0);m.token=8;ingress(&m,NULL,1);
    CHECK(r->lease.owner==1&&!r->state_size);
    // A different peer already loading here still preserves that encounter.
    r=fresh();boss.oHealth=1;size=state_body(body);m=message(r,BN_STATE,1,size);ingress(&m,body,1);
    gNetworkPlayers[0].currLevelNum=16;r->present[2]=0;gNetworkPlayers[2].currAreaSyncValid=false;
    gNetworkPlayers[1].currAreaSyncValid=false;gNetworkPlayers[1].currLevelAreaSeqId++;
    boss_net_update();CHECK(r->lease.owner==BOSS_NET_NO_OWNER&&r->state_size==size);
    gNetworkPlayers[2].currAreaSyncValid=true;m=message(r,BN_JOIN,0,0);ingress(&m,NULL,2);
    CHECK(r->lease.owner==2&&r->state_size==size);

    /* Generic object ingress cannot bypass the authority envelope. */
    struct Packet legacy={0};legacy.packetType=PACKET_OBJECT;legacy.dataLength=size;
    memcpy(legacy.buffer,body,size);CHECK(!boss_net_legacy_packet_valid(&legacy));
    legacy.buffer[9]=42;CHECK(boss_net_legacy_packet_valid(&legacy));
    gCLIOpts.characterNet=false;legacy.buffer[9]=1;CHECK(boss_net_legacy_packet_valid(&legacy));
    CHECK(boss_net_simulates(&boss));
    deferred_reward_tests();
    defeat_dialog_handoff_tests();
    whomp_authority_tests();
    boss_net_reset();
    printf("boss network host: %u checks passed (inert host/transport fixtures)\n",checks);
    return 0;
}
