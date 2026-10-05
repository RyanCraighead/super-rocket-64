/* Real pickup packet writer/reader, simulated independent peers; no sockets,
 * windows, assets, controller or save access. Deliver duplicates deliberately. */
#include <assert.h>
#include <stdio.h>
#include "game/object_list_processor.h"
#include "../../../src/pc/network/packets/packet_collect_coin.c"

struct CLIOptions gCLIOpts;
enum NetworkType gNetworkType;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal,*gNetworkPlayerServer;
struct MarioState gMarioStates[MAX_PLAYERS];
struct LevelValues gLevelValues;
s16 gCurrCourseNum,gCurrActStarNum,gCurrLevelNum,gCurrAreaIndex;
bool gNetworkAreaLoaded;
bool gDjuiInMainMenu;
struct ObjectNode *gObjectLists;
const BehaviorScript bhvRedCoin[]={0},yellow[]={1};
static struct ObjectNode lists[NUM_OBJ_LISTS];
static struct Object coins[2];
static struct Packet sent;
static int sentCount,creditCount,owns,peerId,stars;
static u32 epoch=9;
static float fuel;
static unsigned scoreChecks;
#define SCORE_CHECK(expression) do { scoreChecks++;assert(expression); } while(0)

#ifdef COIN_NATIVE_LIFECYCLE
/* Actual native reset bodies, with only allocator/renderer/other-feature
 * services inert. The production allocator itself clears coin identity. */
struct ObjectNode gFreeObjectList;
struct Object *gCurrentObject;
Vec3f gVec3fZero;
Vec3s gVec3sZero;
Mat4 gMat4Identity={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
static struct Object *allocationTarget;
static struct Object *try_allocate_object(struct ObjectNode *dest,struct ObjectNode *freeList){
    assert(dest&&freeList==&gFreeObjectList);return allocationTarget;
}
struct Object *find_unimportant_object(void){return NULL;}
void unload_object(struct Object *o){(void)o;assert(0);}
void rocket_enemy_forget(struct Object *o){(void)o;}
void rocket_bobomb_forget(struct Object *o){(void)o;}
void rocket_contacts_forget(struct Object *o){(void)o;}
void rocket_platform_forget(struct Object *o){(void)o;}
void rocket_whomp_forget(struct Object *o){(void)o;}
void rocket_switch_forget(struct Object *obj){(void)obj;}
bool gNetworkAreaSyncing;
u16 networkLoadingLevel;
u32 gNetworkAreaTimer,gNetworkAreaTimerClock;
u32 clock_elapsed_ticks(void){return 123;}
static u32 sNextSyncId;
static void *sSoMap;
static unsigned mapClears;
void hmap_clear(void *map){assert(map==sSoMap);mapClears++;}
struct SyncObject *sync_object_get_first(void){return NULL;}
struct SyncObject *sync_object_get_next(void){return NULL;}
void sync_object_forget(u32 id){(void)id;assert(0);}
#include "coin_lifecycle_native.inc"
#endif
enum BehaviorId get_id_from_behavior(const BehaviorScript *b){return b==yellow?1:2;}
const BehaviorScript *get_behavior_from_id(enum BehaviorId id){return id==1?yellow:id==2?bhvRedCoin:NULL;}
const BehaviorScript *smlua_override_behavior(const BehaviorScript *b){return b;}
u32 get_object_list_from_behavior(const BehaviorScript *b){(void)b;return OBJ_LIST_LEVEL;}
void *segmented_to_virtual(const void *p){return (void*)p;}
void bhv_spawn_star_no_level_exit(struct Object *o,u32 a,u8 b){(void)o;(void)a;(void)b;stars++;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;}
int rocket_runtime_owns_controls(void){return owns;}
u32 rocket_runtime_epoch(void){return epoch;}
int rocket_runtime_collect_coin(void){fuel=fminf(100,fuel+5);creditCount++;return 1;}
void network_send(struct Packet *p){sent=*p;sentCount++;}
void network_send_to(u8 index,struct Packet *p){assert(index>0&&index<MAX_PLAYERS);sent=*p;sentCount++;}
struct NetworkPlayer *network_player_from_global_index(u8 global){for(int i=0;i<3;i++)if(gNetworkPlayers[i].globalIndex==global)return &gNetworkPlayers[i];return NULL;}
static int local_index(int global){return global==peerId?0:global==0?peerId:global;}
static void select_peer(int id){
    peerId=id;gNetworkType=id?NT_CLIENT:NT_SERVER;
    memset(gNetworkPlayers,0,sizeof gNetworkPlayers);
    for(int i=0;i<3;i++){
        struct NetworkPlayer *np=&gNetworkPlayers[local_index(i)];
        np->connected=true;np->localIndex=local_index(i);np->globalIndex=i;np->currAreaSyncValid=true;
        np->currCourseNum=gCurrCourseNum;np->currActNum=gCurrActStarNum;
        np->currLevelNum=gCurrLevelNum;np->currAreaIndex=gCurrAreaIndex;np->currLevelAreaSeqId=20+i;
    }
    gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerServer=id?&gNetworkPlayers[local_index(0)]:NULL;
    network_coin_boost_clear(0); // A fresh simulated process/world, not a peer disconnect.
    memset(coinPending,0,sizeof coinPending);memset(coinSeen,0,sizeof coinSeen);
    memset(coins,0,sizeof coins);memset(gMarioStates,0,sizeof gMarioStates);
    for(int i=0;i<NUM_OBJ_LISTS;i++)lists[i].next=&lists[i];
    lists[OBJ_LIST_LEVEL].next=&coins[0].header;coins[0].header.next=&coins[1].header;coins[1].header.next=&lists[OBJ_LIST_LEVEL];
    for(int i=0;i<2;i++){
        coins[i].activeFlags=ACTIVE_FLAG_ACTIVE;coins[i].oInteractType=INTERACT_COIN;
        coins[i].oDamageOrCoinValue=1;coins[i].behavior=yellow;coins[i].oSyncID=100+i;
        coins[i].oPosX=coins[i].oHomeX=100+i*10;
        network_coin_boost_init_identity(&coins[i]);
    }
    fuel=40;creditCount=sentCount=0;owns=1;epoch=9;
}
static void receive(struct Packet packet,int source){
    packet.localIndex=local_index(source);packet.cursor=3;assert(packet_initial_read(&packet));
    network_receive_collect_coin(&packet);
}
static struct Packet pickup(void){
    interact_coin(&gMarioStates[0],INTERACT_COIN,&coins[0]);return sent;
}
static struct Packet event_packet(CoinBoostEvent e){
    struct Packet p={0};packet_init(&p,PACKET_COLLECT_COIN,true,PLMT_LEVEL);coin_event_write(&p,&e);return p;
}
static CoinBoostEvent read_event(struct Packet p){
    CoinBoostEvent e={0};p.cursor=3;assert(packet_initial_read(&p));assert(coin_event_read(&p,&e));return e;
}
static void unsynced_pair(int sharedParent){
    static struct Object parent;memset(&parent,0,sizeof parent);parent.oSyncID=700;
    for(int i=0;i<2;i++){
        coins[i].oSyncID=0;coins[i].coinBoostOrdinal=coins[i].coinBoostChildren=0;
        coins[i].parentObj=sharedParent?&parent:&coins[i];
        coins[i].oPosX=sharedParent?100:100+i*10;
        network_coin_boost_init_identity(&coins[i]);
    }
}
static void score_regressions(void){
    struct Packet claim,rival,second;
    /* Both claim orders, with real local pickups on all three roles. */
    for(int order=0;order<2;order++){
        select_peer(1);claim=pickup();
        select_peer(2);rival=pickup();
        select_peer(0);
        receive(order?rival:claim,order?2:1);receive(order?claim:rival,order?1:2);
        SCORE_CHECK(gMarioStates[0].numCoins==1&&sentCount==1&&creditCount==0);
        select_peer(1);pickup();receive(rival,2);
        SCORE_CHECK(gMarioStates[0].numCoins==1&&creditCount==0);
        select_peer(2);pickup();receive(claim,1);
        SCORE_CHECK(gMarioStates[0].numCoins==1&&creditCount==0);
    }
    select_peer(1);claim=pickup();
    select_peer(0);pickup();receive(claim,1);
    SCORE_CHECK(gMarioStates[0].numCoins==1&&creditCount==1&&sentCount==1);
    select_peer(0);receive(claim,1);pickup();
    SCORE_CHECK(gMarioStates[0].numCoins==1&&creditCount==0&&sentCount==1);
    /* Missing authority does not lose the native local pickup tombstone. */
    select_peer(0);gNetworkAreaLoaded=false;pickup();gNetworkAreaLoaded=true;
    coins[0].activeFlags=ACTIVE_FLAG_DEACTIVATED;receive(claim,1);
    SCORE_CHECK(gMarioStates[0].numCoins==1&&sentCount==0&&creditCount==0);
    /* A later cumulative snapshot must not count a second live object early. */
    for(int order=0;order<2;order++){
        select_peer(1);claim=pickup();interact_coin(&gMarioStates[0],INTERACT_COIN,&coins[1]);second=sent;
        select_peer(0);receive(order?second:claim,1);
        SCORE_CHECK(gMarioStates[0].numCoins==1);
        receive(order?claim:second,1);receive(claim,1);receive(second,1);
        SCORE_CHECK(gMarioStates[0].numCoins==2&&sentCount==2);
    }
    /* Native deletion unlinks the object; a different peer's fresh request
     * and exaggerated cumulative score still cannot resurrect its value. */
    select_peer(1);claim=pickup();
    select_peer(2);rival=pickup();CoinBoostEvent rivalEvent=read_event(rival);rivalEvent.numCoins=99;
    select_peer(0);receive(claim,1);coins[0].activeFlags=ACTIVE_FLAG_DEACTIVATED;
    lists[OBJ_LIST_LEVEL].next=&coins[1].header;
    receive(event_packet(rivalEvent),2);
    SCORE_CHECK(gMarioStates[0].numCoins==1&&sentCount==1);
    network_coin_boost_clear(local_index(2)); // Reused remote slot/request window.
    receive(event_packet(rivalEvent),2);
    SCORE_CHECK(gMarioStates[0].numCoins==1&&sentCount==1);
    /* A reset native status flag alone does not create a new lifetime. */
    select_peer(0);receive(claim,1);coins[0].oInteractStatus=0;
    receive(event_packet(rivalEvent),2);
    SCORE_CHECK(gMarioStates[0].numCoins==1&&sentCount==1);
    /* Authored unsynchronized and shared-parent loot keys survive unlink,
     * without suppressing a distinct sibling at the same spawn position. */
    for(int shared=0;shared<2;shared++){
        select_peer(1);unsynced_pair(shared);claim=pickup();
        interact_coin(&gMarioStates[0],INTERACT_COIN,&coins[1]);second=sent;
        select_peer(2);unsynced_pair(shared);rival=pickup();
        select_peer(0);unsynced_pair(shared);receive(claim,1);
        coins[0].activeFlags=ACTIVE_FLAG_DEACTIVATED;lists[OBJ_LIST_LEVEL].next=&coins[1].header;
        receive(rival,2);receive(second,1);
        SCORE_CHECK(gMarioStates[0].numCoins==2&&sentCount==2&&coinConsumedCount==2);
    }
    /* Native weighted score and one threshold crossing, no duplicate star. */
    select_peer(1);coins[0].oDamageOrCoinValue=5;claim=pickup();
    select_peer(2);coins[0].oDamageOrCoinValue=5;rival=pickup();
    select_peer(0);coins[0].oDamageOrCoinValue=5;gMarioStates[0].numCoins=95;stars=0;
    receive(claim,1);receive(rival,2);
    SCORE_CHECK(gMarioStates[0].numCoins==100&&stars==1&&sentCount==1);
    /* UNKNOWN/unloaded-area totals retain the pre-existing legacy behavior.
     * These are compatibility assertions, not proof of join-baseline dedup. */
    select_peer(1);claim=pickup();CoinBoostEvent unknown=read_event(claim);unknown.syncId=999;unknown.numCoins=7;
    select_peer(0);receive(event_packet(unknown),1);
    SCORE_CHECK(gMarioStates[0].numCoins==7&&sentCount==0&&!coins[0].oInteractStatus);
    select_peer(0);unknown.area=2;gNetworkPlayers[local_index(1)].currAreaIndex=2;
    receive(event_packet(unknown),1);
    SCORE_CHECK(gMarioStates[0].numCoins==7&&sentCount==0&&!coins[0].oInteractStatus);
    /* Saturation never silently evicts a tombstone and reopens an old claim.
     * Exact new coins remain usable; only unprovable claims fail closed. */
    select_peer(1);claim=pickup();
    CoinBoostEvent capacityEvent=read_event(claim);
    select_peer(0);
    for(unsigned i=0;i<COIN_BOOST_CONSUMED;i++){
        capacityEvent.syncId=1000+i;coin_remember_consumed(&capacityEvent,NULL);
    }
    capacityEvent.syncId=1000+COIN_BOOST_CONSUMED;coin_remember_consumed(&capacityEvent,NULL);
    SCORE_CHECK(coinConsumedCount==COIN_BOOST_CONSUMED&&coinConsumedFull);
    receive(claim,1);SCORE_CHECK(gMarioStates[0].numCoins==1&&sentCount==1);
    capacityEvent.request++;receive(event_packet(capacityEvent),1);
    SCORE_CHECK(gMarioStates[0].numCoins==1&&sentCount==1);
#ifdef COIN_NATIVE_LIFECYCLE
    /* Actual allocate_object clears the ordinal; new initialization retires
     * only this address's incarnation, leaving the old immutable tombstone. */
    select_peer(1);claim=pickup();CoinBoostEvent fresh=read_event(claim);
    select_peer(0);receive(claim,1);allocationTarget=&coins[0];
    struct Object *allocated=allocate_object(&lists[OBJ_LIST_LEVEL]);
    SCORE_CHECK(allocated==&coins[0]&&!allocated->coinBoostOrdinal&&!allocated->oSyncID);
    allocated->behavior=yellow;allocated->oSyncID=200;allocated->oInteractType=INTERACT_COIN;
    allocated->oDamageOrCoinValue=1;network_coin_boost_init_identity(allocated);
    fresh.syncId=200;fresh.request++;receive(event_packet(fresh),1);
    SCORE_CHECK(gMarioStates[0].numCoins==2&&sentCount==2);
    fresh.syncId=100;fresh.request++;receive(event_packet(fresh),1);
    SCORE_CHECK(gMarioStates[0].numCoins==2&&sentCount==2);
    allocated=allocate_object(&lists[OBJ_LIST_LEVEL]);
    allocated->behavior=yellow;allocated->oSyncID=100;allocated->oInteractType=INTERACT_COIN;
    allocated->oDamageOrCoinValue=1;network_coin_boost_init_identity(allocated);
    fresh.request++;receive(event_packet(fresh),1);
    SCORE_CHECK(gMarioStates[0].numCoins==3&&sentCount==3); // Actual new native coin.
    /* Actual sync_objects_clear → network_on_init_area clears same-location
     * history and pending grants, even with unchanged course/area numbers. */
    SCORE_CHECK(coinConsumedCount>0);coinPending[0].request=55;sNextSyncId=42;
    networkLoadingLevel=8;gNetworkAreaTimer=400;sync_objects_clear();
    SCORE_CHECK(!coinConsumedCount&&!coinConsumedFull&&!coinPending[0].request);
    SCORE_CHECK(sNextSyncId==SYNC_ID_BLOCK_SIZE/2&&!gNetworkAreaLoaded&&gNetworkAreaSyncing);
    SCORE_CHECK(!networkLoadingLevel&&!gNetworkAreaTimer&&gNetworkAreaTimerClock==123&&mapClears==1);
    gNetworkAreaLoaded=true;
#endif
    printf("coin score: %u race/reorder/tombstone/native-lifetime/baseline-compatibility checks passed\n",scoreChecks);
}
int main(void){
    (void)LuaActionHookTypeArgName;
    gCLIOpts.characterNet=true;gNetworkAreaLoaded=true;gObjectLists=lists;
    gCurrCourseNum=1;gCurrActStarNum=1;gCurrLevelNum=9;gCurrAreaIndex=1;
    gLevelValues.coinsRequiredForCoinStar=100;
    select_peer(1);struct Packet claim=pickup();assert(fuel==40&&sentCount==1);
    CoinBoostPending pending[COIN_BOOST_PENDING];memcpy(pending,coinPending,sizeof pending);
    select_peer(0);receive(claim,1);assert(sentCount==1&&creditCount==0&&fuel==40);
    struct Packet grant=sent;assert(coins[0].oInteractStatus&INT_STATUS_INTERACTED);assert(!coins[1].oInteractStatus);
    receive(claim,1);assert(sentCount==1&&gMarioStates[0].numCoins==1); // duplicate claim
    select_peer(1);memcpy(coinPending,pending,sizeof pending);receive(grant,0);
    assert(fuel==45&&creditCount==1);receive(grant,0);assert(fuel==45&&creditCount==1);
    // Same physical coin collected concurrently by two clients: one grant only.
    select_peer(2);struct Packet rival=pickup();select_peer(0);
    receive(claim,1);receive(rival,2);assert(sentCount==1&&creditCount==0);
    assert(gMarioStates[0].numCoins==1);
    // Authority's local pickup beats a late client request; client receipt alone
    // never refills the observer. Mario also reserves the consumed coin.
    select_peer(0);struct Packet host=pickup();assert(fuel==45&&creditCount==1);
    pickup();assert(creditCount==1&&gMarioStates[0].numCoins==1&&gMarioStates[0].healCounter==4);
    interact_coin(&gMarioStates[1],INTERACT_COIN,&coins[1]);assert(!coins[1].oInteractStatus&&creditCount==1);
    receive(claim,1);assert(sentCount==1&&creditCount==1);
    select_peer(1);receive(host,0);assert(fuel==40&&creditCount==0);
    select_peer(0);owns=0;pickup();receive(claim,1);assert(creditCount==0&&sentCount==1);
    // Grants require a matching pending local request, current authority/area
    // epoch and source. Stale reset/disconnect/warp responses fail closed.
    select_peer(2);receive(grant,0);assert(!creditCount);
    // The real ingress validator rejects forged authority before relay.
    select_peer(0);struct Packet forged=grant;forged.cursor=3;packet_initial_read(&forged);
    forged.localIndex=local_index(2);assert(!network_character_coin_valid(&forged));
    select_peer(1);memcpy(coinPending,pending,sizeof pending);receive(grant,2);assert(!creditCount);
    epoch++;receive(grant,0);assert(!creditCount);
    select_peer(1);memcpy(coinPending,pending,sizeof pending);gNetworkPlayerLocal->currLevelAreaSeqId++;receive(grant,0);assert(!creditCount);
    select_peer(1);memcpy(coinPending,pending,sizeof pending);gNetworkPlayerServer->currLevelAreaSeqId++;receive(grant,0);assert(!creditCount);
    // Full meter clamps. Score-valued red/blue objects still grant just five.
    for(int value=1;value<=5;value++){
        select_peer(1);coins[0].oDamageOrCoinValue=value;struct Packet q=pickup();memcpy(pending,coinPending,sizeof pending);
        select_peer(0);coins[0].oDamageOrCoinValue=value;receive(q,1);struct Packet g=sent;assert(sentCount==1);
        select_peer(1);memcpy(coinPending,pending,sizeof pending);fuel=value==5?99:40;receive(g,0);assert(fuel==(value==5?100:45));
    }
    // Malformed/truncated/nonfinite packets and unknown object identity cannot
    // consume an adjacent live coin or generate fuel grants.
    select_peer(1);claim=pickup();CoinBoostEvent event={0};claim.cursor=3;packet_initial_read(&claim);assert(coin_event_read(&claim,&event));
    select_peer(0);struct Packet bad=event_packet(event);bad.dataLength--;receive(bad,1);assert(!sentCount);
    event.home[0]=NAN;receive(event_packet(event),1);assert(!sentCount);event.home[0]=100;
    event.syncId=999;receive(event_packet(event),1);assert(!sentCount&&!coins[1].oInteractStatus);
    // Unsynchronised coins match exact authored identity; ambiguous matches deny.
    select_peer(1);coins[0].oSyncID=0;claim=pickup();select_peer(0);coins[0].oSyncID=0;
    receive(claim,1);assert(sentCount==1&&!coins[1].oInteractStatus);
    select_peer(0);coins[0].oSyncID=coins[1].oSyncID=0;coins[1].coinBoostParent=coins[0].coinBoostParent;
    memcpy(coins[1].coinBoostOrigin,coins[0].coinBoostOrigin,sizeof coins[0].coinBoostOrigin);
    receive(claim,1);assert(!sentCount);
    // Enemy loot siblings have distinct identities despite identical origins;
    // capture is idempotent and independent of later movement or object reuse.
    select_peer(1);struct Object parent={0};parent.oSyncID=700;
    for(int i=0;i<2;i++){
        coins[i].oSyncID=0;coins[i].parentObj=&parent;coins[i].coinBoostOrdinal=0;
        coins[i].oPosX=100;network_coin_boost_init_identity(&coins[i]);
        network_coin_boost_init_identity(&coins[i]);assert(coins[i].coinBoostOrdinal==(u32)i+1);
    }
    coins[0].parentObj=NULL;coins[0].oPosX+=2000; // Boo-style detachment before pickup.
    claim=pickup();select_peer(0);parent.coinBoostChildren=0;
    for(int i=0;i<2;i++){
        coins[i].oSyncID=0;coins[i].parentObj=&parent;coins[i].coinBoostOrdinal=0;
        network_coin_boost_init_identity(&coins[i]);coins[i].oPosX+=500;
    }
    receive(claim,1);assert(sentCount==1&&!coins[1].oInteractStatus);
    // Host in another area: lowest synchronized area peer is the authority.
    select_peer(1);gNetworkPlayerServer->currAreaIndex=2;pickup();assert(creditCount==1);
    // Reordering window accepts each new event once, retains replay protection.
    CoinBoostSeen seen={0};assert(coin_new_request(&seen,100,7));assert(coin_new_request(&seen,98,7));
    assert(!coin_new_request(&seen,98,7));assert(coin_new_request(&seen,200,7));assert(!coin_new_request(&seen,100,7));
    assert(coin_new_request(&seen,1,8));assert(!coin_new_request(&seen,0,8));
    select_peer(0);coinSeen[1]=seen;coinPending[0].request=123;network_coin_boost_clear(1);
    assert(!coinSeen[1].newest&&!coinPending[0].request);
    assert(coin_new_request(&coinSeen[1],1,8)); // Fresh process reuses global slot/area sequence.
    score_regressions();
    // Offline path grants immediately, no multiplayer snapshot/counter inference.
    select_peer(0);gCLIOpts.characterNet=false;gNetworkPlayerLocal=NULL;network_send_collect_coin(&coins[0]);assert(fuel==45);
    puts("coin boost: PASS actual packet encode/decode, host/client arbitration, duplicates, races, lifecycle, clamp, malformed input");
    return 0;
}
