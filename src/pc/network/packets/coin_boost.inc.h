/* Character-net coin grants. Included only by packet_collect_coin.c.
 * The lowest connected global ID in the loaded area arbitrates pickups (the
 * server when present). Ordinary shared coin-count packets never refill fuel.
 * Native pickup remains immediate; only the boost reward waits for authority.
 */
#include "pc/rocket_runtime.h"
#include "game/area.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

enum { COIN_BOOST_WIRE_SIZE=55, COIN_BOOST_PENDING=64, COIN_BOOST_CONSUMED=4096 };
typedef struct CoinBoostEvent {
    u8 kind,collector,authority;
    u16 areaSequence,authoritySequence;
    u32 request,epoch,syncId,behaviorId;
    u32 parent,ordinal;
    f32 home[3];
    u32 params;
    s16 numCoins;
    s32 value;
    s16 area;
} CoinBoostEvent;
typedef struct CoinBoostPending { u32 request,epoch;u16 areaSequence,authoritySequence;u8 authority; } CoinBoostPending;
typedef struct CoinBoostSeen { u32 newest;uint64_t bits;u16 areaSequence; } CoinBoostSeen;
typedef struct CoinBoostConsumed { CoinBoostEvent key;struct Object *incarnation; } CoinBoostConsumed;
enum CoinBoostMatch { COIN_UNKNOWN,COIN_LIVE,COIN_CONSUMED };
static CoinBoostPending coinPending[COIN_BOOST_PENDING];
static CoinBoostSeen coinSeen[MAX_PLAYERS];
/* Request IDs deduplicate a sender; this area-local history deduplicates the
 * physical coin across senders, including after native deletion/slot reuse.
 * Keep immutable keys, never dereference the saved incarnation pointer. */
static CoinBoostConsumed coinConsumed[COIN_BOOST_CONSUMED];
static unsigned coinConsumedCount;
static int coinConsumedFull;
static u32 coinRequest;
#include "coin_boost_qa.inc.h"

void network_coin_boost_init_identity(struct Object *o) {
    if(!gCLIOpts.characterNet||!o||o->coinBoostOrdinal)return;
    /* allocate_object resets the ordinal. Only that new native lifetime may
     * retire this address's previous incarnation; retain the old wire key. */
    for(unsigned i=0;i<coinConsumedCount;i++)if(coinConsumed[i].incarnation==o)coinConsumed[i].incarnation=NULL;
    struct Object *parent=o->parentObj?o->parentObj:o;
    o->coinBoostParent=parent->oSyncID;
    if(++parent->coinBoostChildren==0)++parent->coinBoostChildren;
    o->coinBoostOrdinal=parent->coinBoostChildren;
    o->coinBoostOrigin[0]=o->oPosX;o->coinBoostOrigin[1]=o->oPosY;o->coinBoostOrigin[2]=o->oPosZ;
}

void network_coin_boost_clear(unsigned index) {
    if(index<MAX_PLAYERS)memset(&coinSeen[index],0,sizeof coinSeen[index]);
    /* A local area baseline (also same-area native reload) retires keys.
     * A remote disconnect/rejoin must not resurrect this world's coins. */
    if(index==0){coinConsumedCount=0;coinConsumedFull=0;}
    // A lost peer may have been authority for a pending pickup. Never carry its
    // grants across reconnect; the next accepted pickup gets a new request ID.
    memset(coinPending,0,sizeof coinPending);
}

static int coin_same_area(const struct NetworkPlayer *np) {
    return np&&np->connected&&np->currAreaSyncValid&&np->currCourseNum==gCurrCourseNum&&
        np->currActNum==gCurrActStarNum&&np->currLevelNum==gCurrLevelNum&&np->currAreaIndex==gCurrAreaIndex;
}
static struct NetworkPlayer *coin_authority(void) {
    if(!gNetworkAreaLoaded||!coin_same_area(gNetworkPlayerLocal))return NULL;
    struct NetworkPlayer *owner=gNetworkPlayerLocal;
    for(int i=0;i<MAX_PLAYERS;i++)if(coin_same_area(&gNetworkPlayers[i])&&gNetworkPlayers[i].globalIndex<owner->globalIndex)owner=&gNetworkPlayers[i];
    return owner;
}
static int coin_new_request(CoinBoostSeen *seen,u32 request,u16 areaSequence) {
    if(!request)return 0;
    if(seen->areaSequence!=areaSequence){memset(seen,0,sizeof *seen);seen->areaSequence=areaSequence;}
    if(!seen->newest){seen->newest=request;seen->bits=1;return 1;}
    int32_t delta=(int32_t)(request-seen->newest);
    if(delta>0){seen->bits=delta>=64?1:(seen->bits<<delta)|1;seen->newest=request;return 1;}
    u32 back=seen->newest-request;
    if(back>=64||(seen->bits&(UINT64_C(1)<<back)))return 0;
    seen->bits|=UINT64_C(1)<<back;return 1;
}
static void coin_event_write(struct Packet *p,CoinBoostEvent *e) {
#define COIN_WRITE(field) packet_write(p,&e->field,sizeof e->field)
    COIN_WRITE(kind);COIN_WRITE(collector);COIN_WRITE(authority);
    COIN_WRITE(areaSequence);COIN_WRITE(authoritySequence);COIN_WRITE(request);COIN_WRITE(epoch);
    COIN_WRITE(syncId);COIN_WRITE(behaviorId);COIN_WRITE(parent);COIN_WRITE(ordinal);COIN_WRITE(home);COIN_WRITE(params);
    COIN_WRITE(numCoins);COIN_WRITE(value);COIN_WRITE(area);
#undef COIN_WRITE
}
static int coin_event_read(struct Packet *p,CoinBoostEvent *e) {
    if(p->error||p->cursor+COIN_BOOST_WIRE_SIZE!=p->dataLength)return 0;
#define COIN_READ(field) packet_read(p,&e->field,sizeof e->field)
    COIN_READ(kind);COIN_READ(collector);COIN_READ(authority);
    COIN_READ(areaSequence);COIN_READ(authoritySequence);COIN_READ(request);COIN_READ(epoch);
    COIN_READ(syncId);COIN_READ(behaviorId);COIN_READ(parent);COIN_READ(ordinal);COIN_READ(home);COIN_READ(params);
    COIN_READ(numCoins);COIN_READ(value);COIN_READ(area);
#undef COIN_READ
    return !p->error&&e->kind<=1&&e->request&&e->value>0&&e->value<=255&&e->numCoins>=0&&
        isfinite(e->home[0])&&isfinite(e->home[1])&&isfinite(e->home[2]);
}
static int coin_key_matches(struct Object *o,const CoinBoostEvent *e) {
    if(o->activeFlags==ACTIVE_FLAG_DEACTIVATED||!(o->oInteractType&INTERACT_COIN)||
       o->oDamageOrCoinValue!=e->value||(u32)o->oBehParams!=e->params)return 0;
    if(get_id_from_behavior(o->behavior)!=e->behaviorId)return 0;
    if(e->syncId)return o->oSyncID==e->syncId;
    if(o->oSyncID||!e->ordinal||o->coinBoostParent!=e->parent||o->coinBoostOrdinal!=e->ordinal)return 0;
    if(e->parent)return 1; // Parent sync ID + coin-only spawn ordinal survive motion/jitter.
    return o->coinBoostOrigin[0]==e->home[0]&&o->coinBoostOrigin[1]==e->home[1]&&o->coinBoostOrigin[2]==e->home[2];
}
static int coin_same_key(const CoinBoostEvent *a,const CoinBoostEvent *b) {
    if(a->area!=b->area||a->syncId!=b->syncId||a->behaviorId!=b->behaviorId||
       a->params!=b->params||a->value!=b->value)return 0;
    if(a->syncId)return 1;
    if(!a->ordinal||a->parent!=b->parent||a->ordinal!=b->ordinal)return 0;
    return a->parent||(a->home[0]==b->home[0]&&a->home[1]==b->home[1]&&a->home[2]==b->home[2]);
}
static CoinBoostConsumed *coin_consumed_find(const CoinBoostEvent *e) {
    for(unsigned i=0;i<coinConsumedCount;i++)if(coin_same_key(&coinConsumed[i].key,e))return &coinConsumed[i];
    return NULL;
}
static void coin_remember_consumed(const CoinBoostEvent *e,struct Object *o) {
    CoinBoostConsumed *entry=coin_consumed_find(e);
    if(!entry){
        /* Never evict a known consumed key. At pathological capacity, unknown
         * same-area claims fail closed while exact new live coins still work. */
        if(coinConsumedCount==COIN_BOOST_CONSUMED){coinConsumedFull=1;return;}
        entry=&coinConsumed[coinConsumedCount++];
    }
    entry->key=*e;entry->incarnation=o;
}
static enum CoinBoostMatch coin_find_exact(const CoinBoostEvent *e,struct Object **live) {
    extern struct ObjectNode *gObjectLists;
    *live=NULL;
    CoinBoostConsumed *known=coin_consumed_find(e);
    const BehaviorScript *behavior=get_behavior_from_id(e->behaviorId);
    if(!gObjectLists||!behavior)return known||coinConsumedFull?COIN_CONSUMED:COIN_UNKNOWN;
    struct ObjectNode *head=&gObjectLists[get_object_list_from_behavior(smlua_override_behavior(behavior))];
    struct Object *found=NULL,*consumed=NULL;
    int ambiguous=0;
    for(struct Object *o=(struct Object*)head->next;o!=(struct Object*)head;o=(struct Object*)o->header.next){
        if(!coin_key_matches(o,e))continue;
        if(found)ambiguous=1;
        if(o->oInteractStatus&INT_STATUS_INTERACTED)consumed=o;
        found=o;
    }
    if(consumed){coin_remember_consumed(e,consumed);return COIN_CONSUMED;}
    if(ambiguous)return known||coinConsumedFull?COIN_CONSUMED:COIN_UNKNOWN;
    if(found){
        if(known&&known->incarnation==found)return COIN_CONSUMED;
        *live=found;return COIN_LIVE;
    }
    return known||coinConsumedFull?COIN_CONSUMED:COIN_UNKNOWN;
}
static int coin_source_valid(struct Packet *p,u8 source) {
    if(p->localIndex==0||p->localIndex>=MAX_PLAYERS||!gNetworkPlayers[p->localIndex].connected)return 0;
    if(gNetworkPlayers[p->localIndex].globalIndex==source)return 1;
    return gNetworkType==NT_CLIENT&&gNetworkPlayerServer&&p->localIndex==gNetworkPlayerServer->localIndex;
}
/* Validate origin before the server can relay a claim/grant. A forwarded grant
 * must never turn an arbitrary client's sender identity into server authority. */
bool network_character_coin_valid(struct Packet *p) {
    if(!gCLIOpts.characterNet)return true;
    CoinBoostEvent e={0};u16 cursor=p->cursor;
    int valid=coin_event_read(p,&e);p->cursor=cursor;
    if(!valid||!p->levelMustMatch||p->levelAreaMustMatch)return false;
    u8 source=e.kind?e.authority:e.collector;
    if(!coin_source_valid(p,source))return false;
    struct NetworkPlayer *np=network_player_from_global_index(source);
    return np&&np->connected&&np->currCourseNum==p->courseNum&&np->currActNum==p->actNum&&
        np->currLevelNum==p->levelNum&&np->currAreaIndex==e.area;
}
static void coin_send_event(CoinBoostEvent *e,int destination) {
    struct Packet p={0};packet_init(&p,PACKET_COLLECT_COIN,true,PLMT_LEVEL);coin_event_write(&p,e);
    if(destination<0)network_send(&p);else network_send_to((u8)destination,&p);
}
static void coin_boost_send(struct Object *o) {
    CoinBoostEvent e={0};e.syncId=o->oSyncID;e.behaviorId=get_id_from_behavior(o->behavior);
    e.parent=o->coinBoostParent;e.ordinal=o->coinBoostOrdinal;
    memcpy(e.home,o->coinBoostOrigin,sizeof e.home);e.params=o->oBehParams;
    e.numCoins=gMarioStates[0].numCoins;e.value=o->oDamageOrCoinValue;e.area=gCurrAreaIndex;
    // Native interact_coin already added this value. Reserve its exact key
    // before sending, even if area authority is temporarily unavailable.
    coin_remember_consumed(&e,o);
    struct NetworkPlayer *authority=coin_authority();
    if(!authority)return;
    e.collector=gNetworkPlayerLocal->globalIndex;e.authority=authority->globalIndex;
    e.areaSequence=gNetworkPlayerLocal->currLevelAreaSeqId;e.authoritySequence=authority->currLevelAreaSeqId;
    if(++coinRequest==0)++coinRequest;
    e.request=coinRequest;e.epoch=rocket_runtime_epoch();
#ifdef ROCKET_CAR_QA
    coin_qa_event("pickup",&e);
#endif
    if(rocket_runtime_owns_controls()){
        if(authority==gNetworkPlayerLocal)COIN_APPLY_CREDIT(&e);
        else coinPending[e.request%COIN_BOOST_PENDING]=(CoinBoostPending){e.request,e.epoch,e.areaSequence,e.authoritySequence,e.authority};
    }
#ifdef ROCKET_CAR_QA
    coin_qa_event("claim_send",&e);
#endif
    coin_send_event(&e,-1);
}
static void coin_boost_receive(struct Packet *p) {
    CoinBoostEvent e={0};
    if(!network_character_coin_valid(p))return;
    if(!coin_event_read(p,&e)||!gNetworkPlayerLocal)return;
    struct NetworkPlayer *collector=network_player_from_global_index(e.collector);
    struct NetworkPlayer *authority=coin_authority();
    if(!collector||!collector->connected||collector->currLevelAreaSeqId!=e.areaSequence)return;
    if(e.kind==1){
        if(collector!=gNetworkPlayerLocal||!authority||e.authority!=authority->globalIndex||
           e.authoritySequence!=authority->currLevelAreaSeqId||e.area!=gCurrAreaIndex||!coin_source_valid(p,e.authority))return;
#ifdef ROCKET_CAR_QA
        coin_qa_event("grant_received",&e);
#endif
        CoinBoostPending *pending=&coinPending[e.request%COIN_BOOST_PENDING];
        if(pending->request!=e.request||pending->epoch!=e.epoch||pending->areaSequence!=e.areaSequence||
           pending->authority!=e.authority||pending->authoritySequence!=e.authoritySequence)return;
        pending->request=0; // Consume before applying: a replay cannot refill twice.
        if(e.epoch==rocket_runtime_epoch())COIN_APPLY_CREDIT(&e);
        return;
    }
    if(collector==gNetworkPlayerLocal||!coin_source_valid(p,e.collector)||
       !coin_new_request(&coinSeen[collector->localIndex],e.request,e.areaSequence))return;
    s16 oldNumCoins=gMarioStates[0].numCoins;
    int exactLive=0;
    if(e.area==gCurrAreaIndex&&coin_same_area(collector)){
        struct Object *coin=NULL;
        enum CoinBoostMatch match=coin_find_exact(&e,&coin);
        if(match==COIN_CONSUMED)return; // Neither value nor remote total is new evidence.
        if(match==COIN_LIVE){
            exactLive=1;
            coin_remember_consumed(&e,coin);
            coin->oInteractStatus=INT_STATUS_INTERACTED;
#ifdef ROCKET_CAR_QA
            coin_qa_event("claim_accept",&e);
#endif
            if(authority==gNetworkPlayerLocal&&e.authority==authority->globalIndex&&e.authoritySequence==authority->currLevelAreaSeqId){
                e.kind=1;
#ifdef ROCKET_CAR_QA
                coin_qa_event("grant_send",&e);
#endif
                coin_send_event(&e,collector->localIndex);
            }
        }
    }
    /* Exact live objects contribute their native value once. A sender's
     * cumulative total may include other claims arriving later/out of order.
     * Unknown/unloaded-area legacy score reconciliation remains unchanged;
     * it is not proof of an exact pickup and never grants boost. */
    gMarioStates[0].numCoins=exactLive?gMarioStates[0].numCoins+e.value:
        max(e.numCoins,gMarioStates[0].numCoins+e.value);
    if(COURSE_IS_MAIN_COURSE(gCurrCourseNum)&&oldNumCoins<gLevelValues.coinsRequiredForCoinStar&&
       gMarioStates[0].numCoins>=gLevelValues.coinsRequiredForCoinStar&&gCurrAreaIndex==e.area)
        bhv_spawn_star_no_level_exit(gMarioStates[collector->localIndex].marioObj,6,FALSE);
}
