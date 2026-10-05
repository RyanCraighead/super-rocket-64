/* Production packet receive/object apply and platform preflight, with inert world
 * and transport services. Native boss/cap ingress predicates are sliced verbatim. */
#define ROCKET_PLATFORM_REAL_TEST
#include "character_transport_fixture.h"
#include "../../../src/game/rocket_platform.c"
#define network_send_object platform_source_send_object
#include "../../../src/pc/network/packets/packet_object.c"
#undef network_send_object
static unsigned bossPackets,boxPackets;
static int applying;
int boss_net_enabled(void){return 1;}
int boss_net_applying(void){return applying;}
int boss_net_managed(const struct Object *o){(void)o;return 0;}
int boss_net_simulates(const struct Object *o){(void)o;return 1;}
int boss_net_object_packet(struct Object *o,struct Packet *p){(void)o;(void)p;return 0;}
void boss_net_receive(struct Packet *p){(void)p;bossPackets++;}
void network_send_object(struct Object *o){(void)o;boxPackets++;}
#include "platform_ingress_guards.inc"
const BehaviorScript bhvSeesawPlatform[]={1},bhvSwingPlatform[]={2},bhvTTC2DRotator[]={3},
 bhvTTCCog[]={4},bhvTTCElevator[]={5},bhvTTCMovingBar[]={6},bhvTTCPendulum[]={7},
 bhvTTCPitBlock[]={8},bhvTTCRotatingSolid[]={9},bhvTTCSpinner[]={10},
 bhvWdwSquareFloatingPlatform[]={11},bhvWdwRectangularFloatingPlatform[]={12},
 bhvJrbFloatingPlatform[]={13},bhvLllTiltingInvertedPyramid[]={14},bhvBitfsTiltingInvertedPyramid[]={15},
 bhvKingBobomb[]={16},bhvBowser[]={17},bhvExclamationBox[]={18},bhvWingCap[]={19},bhvMetalCap[]={20},bhvVanishCap[]={21},
 bhvPenguinBaby[]={22},bhvSmallPenguin[]={23},bhvCoinFormationSpawn[]={24},bhvYellowCoin[]={25},bhvRespawner[]={26},
 bhvWhompKingBoss[]={27},bhvSmallWhomp[]={28},bhvSingleCoinGetsSpawned[]={29};
static struct Object platform,box;
static struct SyncObject platformSync,boxSync;
static struct Area fixtureArea;
struct Area *gCurrentArea;
struct Object *gCurrentObject;
const BehaviorScript *get_behavior_from_id(enum BehaviorId id){return id==1?bhvSeesawPlatform:id==16?bhvKingBobomb:id==18?bhvExclamationBox:NULL;}
enum BehaviorId get_id_from_behavior(const BehaviorScript *b){return b==bhvSeesawPlatform?1:b==bhvKingBobomb?16:18;}
const char *get_behavior_name_from_id(enum BehaviorId id){(void)id;return "fixture";}
const BehaviorScript *smlua_override_behavior(const BehaviorScript *b){return b;}
struct SyncObject *sync_object_get(u32 id){return id==10?&platformSync:id==20?&boxSync:NULL;}
bool sync_object_is_initialized(u32 id){return sync_object_get(id)!=NULL;}
void sync_object_forget(u32 id){(void)id;}
f32 clock_elapsed(void){return (f32)fixtureNow;}
u8 is_player_active(struct MarioState *m){return m&&m->marioObj&&m->health>=0x100;}
int rocket_adapter_car_selected(void){return gCLIOpts.rocketCar;}
int rocket_adapter_pickup_pose(RocketSnapshot *s){(void)s;return 0;}
int rocket_adapter_cap_box_pose_contact(const RocketSnapshot *c,struct Object *o,unsigned flags){(void)c;(void)o;(void)flags;return 1;}
u32 determine_interaction(struct MarioState *m,struct Object *o){(void)m;(void)o;return INT_HIT_FROM_BELOW;}
static bool match(void *a,void *b){return a==b;}
static void setup(void){
 memset(gNetworkPlayers,0,sizeof gNetworkPlayers);memset(gMarioStates,0,sizeof gMarioStates);
 memset(&platform,0,sizeof platform);memset(&box,0,sizeof box);memset(&platformSync,0,sizeof platformSync);memset(&boxSync,0,sizeof boxSync);
 gCLIOpts.characterNet=true;gCLIOpts.offline=false;gNetworkType=NT_SERVER;gNetworkAreaLoaded=true;
 gNetworkPlayerLocal=gNetworkPlayerServer=&gNetworkPlayers[0];gNetworkServerAddr=NULL;
 gCurrCourseNum=gCurrActStarNum=gCurrLevelNum=0;gCurrAreaIndex=1;fixtureArea.index=1;gCurrentArea=&fixtureArea;
 for(unsigned i=0;i<3;i++){
  struct NetworkPlayer *n=&gNetworkPlayers[i];n->connected=true;n->localIndex=i;n->globalIndex=i;
  n->currPositionValid=n->currLevelSyncValid=n->currAreaSyncValid=true;n->currAreaIndex=1;
  gMarioStates[i].playerIndex=i;gMarioStates[i].marioObj=&objects[i];gMarioStates[i].health=0x880;
  gMarioStates[i].action=ACT_IDLE;gMarioStates[i].area=&fixtureArea;
 }
 /* A lower global-index peer owns this area; native owner selection is real. */
 gNetworkPlayers[0].globalIndex=3;
 platform.behavior=bhvSeesawPlatform;platform.oSyncID=10;platform.activeFlags=ACTIVE_FLAG_ACTIVE;platform.header.gfx.activeAreaIndex=1;
 platformSync.o=&platform;platformSync.id=10;platformSync.behavior=(void*)platform.behavior;platformSync.override_ownership=platform_ownership;
 platformSync.hasStandardFields=true;platformSync.randomSeed=99;platformSync.clockSinceUpdate=17;
 box.behavior=bhvExclamationBox;box.oSyncID=20;box.activeFlags=ACTIVE_FLAG_ACTIVE;box.header.gfx.activeAreaIndex=1;
 box.oAction=2;box.hitboxRadius=40;box.hitboxHeight=30;boxSync.o=&box;boxSync.id=20;boxSync.behavior=(void*)box.behavior;
 forwarded=bossPackets=boxPackets=0;character_net_clear_all();
}
static struct Packet update(unsigned physical,unsigned claimed,unsigned destination,int broadcast){
 struct Packet p={0};packet_init(&p,PACKET_OBJECT,false,PLMT_AREA);
 u8 origin=claimed;u32 sync=10,behavior=1;u16 event=42,seed=123;
 packet_write(&p,&origin,1);packet_write(&p,&sync,4);packet_write(&p,&event,2);packet_write(&p,&seed,2);packet_write(&p,&behavior,4);
 float old=platform.oPosY;platform.oPosY=123;packet_write_object_standard_fields(&p,&platform);platform.oPosY=old;
 packet_write_object_extra_fields(&p,&platform);
 p.requestBroadcast=broadcast;packet_set_flags(&p);packet_set_destination(&p,destination);p.localIndex=physical;p.addr=(void*)(uintptr_t)physical;p.cursor=3;
 return p;
}
static void unchanged(void){CHECK(platform.oPosY==0&&platformSync.clockSinceUpdate==17&&platformSync.randomSeed==99&&platformSync.rxEventId[1]==0&&platformSync.rxEventId[2]==0);}
int main(void){
 struct NetworkSystem transport={0};transport.requireServerBroadcast=true;transport.get_id_str=id;transport.match_addr=match;gNetworkSystem=&transport;
 setup();struct Packet p=update(2,1,1,0);packet_receive(&p);CHECK(forwarded==0);unchanged(); // Directed forgery.
 p=update(2,1,PACKET_DESTINATION_BROADCAST,1);packet_receive(&p);CHECK(!p.requestBroadcast&&forwarded==0);unchanged();
 p=update(2,2,PACKET_DESTINATION_BROADCAST,1);packet_receive(&p);CHECK(forwarded==0);unchanged(); // Honest origin, nonowner.
 p=update(1,1,PACKET_DESTINATION_BROADCAST,1);p.dataLength=15;packet_receive(&p);CHECK(forwarded==0);unchanged();
 p=update(1,1,PACKET_DESTINATION_BROADCAST,1);p.buffer[9]=2;packet_receive(&p);CHECK(forwarded==0);unchanged(); // Wrong area.
 p=update(1,1,PACKET_DESTINATION_BROADCAST,1);packet_receive(&p);CHECK(forwarded==1&&platform.oPosY==123&&platformSync.randomSeed==123&&platformSync.rxEventId[1]==42);
 setup();p=update(1,1,2,0);packet_receive(&p);CHECK(forwarded==1);unchanged(); // Legitimate directed relay leaves host state untouched.
 setup();p=update(1,1,PACKET_DESTINATION_BROADCAST,0);CHECK(packet_initial_read(&p));CHECK(rocket_platform_packet_allowed(&p));
 gNetworkPlayers[2].globalIndex=0;network_receive_object(&p);unchanged(); // Authority changed in ordered/delayed queue.
 setup();gNetworkType=NT_CLIENT;gNetworkPlayerServer=&gNetworkPlayers[1];gNetworkPlayers[0].globalIndex=3;gNetworkPlayers[1].globalIndex=0;
 p=update(2,0,PACKET_DESTINATION_BROADCAST,0);packet_receive(&p);unchanged();
 p=update(1,0,PACKET_DESTINATION_BROADCAST,0);packet_receive(&p);CHECK(platform.oPosY==123);
 setup();gNetworkType=NT_CLIENT;transport.requireServerBroadcast=false;gNetworkPlayerServer=&gNetworkPlayers[2];
 p=update(2,1,PACKET_DESTINATION_BROADCAST,0);packet_receive(&p);unchanged();
 p=update(1,1,PACKET_DESTINATION_BROADCAST,0);packet_receive(&p);CHECK(platform.oPosY==123);
 setup();transport.requireServerBroadcast=true;p=update(1,1,PACKET_DESTINATION_BROADCAST,1);
 u32 behavior=16,unknownSync=999;memcpy(p.buffer+11,&unknownSync,4);memcpy(p.buffer+19,&behavior,4);packet_receive(&p);CHECK(forwarded==0);unchanged(); // Actual boss legacy guard remains before relays.
 packet_init(&p,PACKET_BOSS_STATE,false,PLMT_AREA);p.localIndex=1;p.cursor=3;packet_receive(&p);CHECK(bossPackets==1&&forwarded==0);
 setup();gNetworkPlayers[0].currAreaIndex=1;CharacterNetState mario={0};mario.kind=CNET_MARIO;mario.sequence=1;CHECK(character_net_accept(1,&mario));
 p=update(1,1,PACKET_DESTINATION_BROADCAST,1);u32 sync=20;behavior=18;memcpy(p.buffer+11,&sync,4);memcpy(p.buffer+19,&behavior,4);
 packet_receive(&p);CHECK(box.oExclamationBoxForce&&boxPackets==1&&forwarded==0&&!p.requestBroadcast);unchanged();
 printf("platform authenticated pre-relay/native-apply ingress: %d checks passed\n",checks);return 0;
}
