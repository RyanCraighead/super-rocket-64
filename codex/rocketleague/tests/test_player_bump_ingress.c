/* Actual packet_receive: rejection must happen before directed/broadcast relay. */
#define PLAYER_BUMP_REAL_TEST
#include "character_transport_fixture.h"
#include "../../../src/pc/player_bump.c"
bool gNetworkAreaSyncing;
struct Area *gCurrentArea;
struct WarpTransition gWarpTransition;
struct WarpDest sWarpDest;
s16 sCurrPlayMode,sDelayedWarpOp;
u32 gGlobalTimer,gTimeStopState;
static struct Area area;
int rocket_adapter_car_selected(void){return gCLIOpts.rocketCar;}
int rocket_adapter_body_snapshot(struct Object *o,RocketSnapshot *s){(void)o;(void)s;return 0;}
int rocket_adapter_whomp_path_clear(const float a[3],const float b[3],struct Object *o){(void)a;(void)b;(void)o;return 1;}
int rocket_runtime_bump(const float delta[3]){(void)delta;abort();}
void mario_set_forward_vel(struct MarioState *m,float speed){(void)m;(void)speed;abort();}
static bool match(void *a,void *b){return a==b;}
static void setup(void){
 memset(gNetworkPlayers,0,sizeof gNetworkPlayers);memset(&bumps,0,sizeof bumps);
 gCLIOpts.characterNet=true;gNetworkType=NT_SERVER;gNetworkAreaLoaded=true;gNetworkAreaSyncing=false;
 gNetworkPlayerLocal=gNetworkPlayerServer=&gNetworkPlayers[0];gCurrentArea=&area;
 gCurrCourseNum=gCurrActStarNum=1;gCurrLevelNum=9;gCurrAreaIndex=1;
 for(unsigned i=0;i<3;i++){
  struct NetworkPlayer*n=&gNetworkPlayers[i];n->connected=true;n->localIndex=i;n->globalIndex=i;
  n->currLevelSyncValid=n->currAreaSyncValid=n->currPositionValid=true;n->currCourseNum=n->currActNum=1;
  n->currLevelNum=9;n->currAreaIndex=1;n->currLevelAreaSeqId=100+i;
 }
 /* Server is in another area. Client 1 legitimately owns client 2's area. */
 gNetworkPlayers[0].currAreaIndex=2;forwarded=0;
}
static struct Packet grant(unsigned from,int broadcast){
 BumpEvent e={0};e.authority=1;e.target=2;e.other=1;e.authorityArea=101;e.targetArea=102;e.otherArea=101;
 e.event=e.targetEpoch=e.otherEpoch=e.targetSequence=e.otherSequence=1;e.targetKind=e.otherKind=CNET_OCTANE;e.delta[0]=600;
 struct Packet p={0};packet_init(&p,PACKET_ROCKET_PLAYER_BUMP,true,PLMT_LEVEL);bump_write(&p,&e);
 p.requestBroadcast=broadcast;packet_set_flags(&p);packet_set_destination(&p,2);p.localIndex=from;p.cursor=3;p.addr=(void*)(uintptr_t)from;return p;
}
int main(void){
 struct NetworkSystem transport={0};transport.requireServerBroadcast=true;transport.get_id_str=id;transport.match_addr=match;gNetworkSystem=&transport;
 setup();struct Packet p=grant(2,0);packet_receive(&p);CHECK(!forwarded);
 setup();p=grant(2,1);packet_receive(&p);CHECK(!forwarded&&!p.requestBroadcast);
 setup();p=grant(1,0);p.dataLength--;packet_receive(&p);CHECK(!forwarded);
 setup();p=grant(1,0);gNetworkPlayers[1].currAreaSyncValid=false;packet_receive(&p);CHECK(!forwarded);
 setup();p=grant(1,0);gNetworkPlayers[2].currAreaIndex=2;packet_receive(&p);CHECK(!forwarded);
 setup();p=grant(1,0);gNetworkPlayers[0].currAreaIndex=1;packet_receive(&p);CHECK(!forwarded);
 setup();p=grant(1,0);packet_receive(&p);CHECK(forwarded==1);
 setup();gNetworkType=NT_CLIENT;gNetworkPlayerLocal=&gNetworkPlayers[2];gNetworkPlayerServer=&gNetworkPlayers[0];
 p=grant(1,0);CHECK(packet_initial_read(&p)&&player_bump_packet_allowed(&p));
 p=grant(2,0);CHECK(packet_initial_read(&p)&&!player_bump_packet_allowed(&p));
 printf("PASS %d actual bump ingress checks: forged authority, directed/broadcast relay, area ownership and valid routing\n",checks);
}
