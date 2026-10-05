/* Real ingress and ownership regression checks; no gameplay or sockets. */
#include "character_transport_fixture.h"
static u32 packetCaps;
static struct Packet sample(u8 sender,u8 claimed,u8 destination,u32 sequence,int active){
 struct Packet p={0};packet_init(&p,PACKET_PLAYER,false,PLMT_AREA);
 p.localIndex=sender;p.requestBroadcast=true;p.destGlobalId=destination;
 p.buffer[3]=3;p.buffer[4]=destination;
 packet_write(&p,&claimed,1);
 struct PacketPlayerData data={0};data.action=ACT_IDLE;data.health=0x880;data.levelSyncValid=data.areaSyncValid=1;
 data.flags=packetCaps;
 packet_write(&p,&data,sizeof data);
 CharacterNetState s={0};s.sequence=sequence;s.kind=CNET_OCTANE;s.active=active;
 s.interaction=active==CNET_DRIVING;
 s.car.basis[0]=s.car.basis[4]=s.car.basis[8]=1;
 for(int i=0;i<4;i++)s.car.wheel_radius[i]=32;
 u8 wire[CNET_WIRE_SIZE];CHECK(character_net_encode(wire,sizeof wire,&s));packet_write(&p,wire,sizeof wire);
 p.cursor=3;return p;
}
#include "test_online_switch_transport.inc.c"
static void test_native_caps(void){
 character_net_clear_all();gNetworkType=NT_SERVER;
 gNetworkPlayerLocal=gNetworkPlayerServer=&gNetworkPlayers[0];
 for(int i=0;i<3;i++){
  gNetworkPlayers[i].globalIndex=i;gNetworkPlayers[i].currPositionValid=true;
  gNetworkPlayers[i].currLevelSyncValid=true;gNetworkPlayers[i].currAreaSyncValid=true;
 }
 const float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};const int viewport[4]={0,0,640,480};
 gMarioStates[0].flags=MARIO_METAL_CAP; // Remote Vanish must never change local permissions.
 for(unsigned sequence=1;sequence<=4;sequence++){
  struct Packet p=sample(1,1,PACKET_DESTINATION_BROADCAST,sequence,sequence!=4);
  struct PacketPlayerData data;size_t at=p.dataLength-CNET_WIRE_SIZE-sizeof data;memcpy(&data,p.buffer+at,sizeof data);
  data.flags=sequence==1?MARIO_VANISH_CAP|MARIO_WING_CAP:0;
  verifiedCapFlags[1]=data.flags;
  memcpy(p.buffer+at,&data,sizeof data);
  int before=accepted;packet_receive(&p);CHECK(accepted==before+1);
  CHECK(gMarioStates[0].flags==MARIO_METAL_CAP);
  unsigned oldDraw=drawCalls;character_net_draw(matrix,matrix,viewport);
  CHECK(drawCalls==oldDraw+(sequence!=4));
  if(sequence!=4)CHECK((drawnCaps&MARIO_SPECIAL_CAPS)==data.flags);
  if(sequence==1){
   // An owner-spoofed cap-bearing pose cannot mutate the other player or renderer.
   p=sample(2,1,PACKET_DESTINATION_BROADCAST,9,1);before=accepted;packet_receive(&p);CHECK(accepted==before);
   CHECK((gMarioStates[1].flags&MARIO_SPECIAL_CAPS)==(MARIO_VANISH_CAP|MARIO_WING_CAP));
  }
 }
 character_net_clear_all();unsigned before=drawCalls;character_net_draw(matrix,matrix,viewport);CHECK(drawCalls==before);
}
int main(void){
 static struct NetworkSystem system={.get_id_str=id,.requireServerBroadcast=true};gNetworkSystem=&system;
 gCLIOpts.characterNet=true;gNetworkType=NT_SERVER;gNetworkAreaLoaded=true;
 for(int i=0;i<3;i++){
  gNetworkPlayers[i].connected=true;gNetworkPlayers[i].localIndex=gNetworkPlayers[i].globalIndex=i;
  gNetworkPlayers[i].currAreaSyncValid=gNetworkPlayers[i].currLevelSyncValid=true;
  gMarioStates[i].playerIndex=i;gMarioStates[i].marioObj=&objects[i];gMarioStates[i].controller=&controllers[i];
 }
 gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerServer=&gNetworkPlayers[0];
 struct Packet p=sample(1,1,PACKET_DESTINATION_BROADCAST,1,1);
 struct PacketPlayerData clientWater={0};clientWater.action=ACT_WATER_IDLE;clientWater.health=0x800;
 clientWater.levelSyncValid=clientWater.areaSyncValid=1;
 memcpy(p.buffer+p.dataLength-CNET_WIRE_SIZE-sizeof clientWater,&clientWater,sizeof clientWater);
 packet_receive(&p);
 CHECK(accepted==1&&forwarded==1);CHECK(objects[1].oIntangibleTimer==-1);CHECK(gMarioStates[0].action==0);
 /* Contacts must use current raw state, with stricter freshness than rendering. */
 gNetworkPlayers[1].currPositionValid=true;
 gNetworkPlayers[1].currLevelSyncValid=gNetworkPlayers[1].currAreaSyncValid=true;
 gNetworkPlayerLocal->currLevelSyncValid=gNetworkPlayerLocal->currAreaSyncValid=true;
 CharacterNetState contact;
 CHECK(character_net_interaction_state(1,&contact,NULL)&&contact.sequence==1);
 double priorTime=fixtureNow;fixtureNow+=.251;CHECK(!character_net_interaction_state(1,&contact,NULL));fixtureNow=priorTime;
 gNetworkPlayers[1].currLevelAreaSeqId++;CHECK(!character_net_interaction_state(1,&contact,NULL));gNetworkPlayers[1].currLevelAreaSeqId--;
 gNetworkPlayers[1].connected=false;CHECK(!character_net_interaction_state(1,&contact,NULL));gNetworkPlayers[1].connected=true;
 CHECK(character_net_water_mode(1)==ROCKET_WATER_JET);
 CHECK(rocket_water_native_mode(ACT_METAL_WATER_FALLING,0)==ROCKET_WATER_JET);
 /* Forged directed delivery must be rejected before transport forwarding. */
 p=sample(1,2,2,2,1);int before=forwarded;packet_receive(&p);CHECK(forwarded==before&&accepted==1);
 /* Forged broadcast from a different area must also be rejected before relay. */
 p=sample(1,2,PACKET_DESTINATION_BROADCAST,2,1);p.buffer[5]=4;packet_receive(&p);CHECK(forwarded==before&&accepted==1);
 p=sample(1,1,PACKET_DESTINATION_BROADCAST,2,1);p.dataLength--;packet_receive(&p);CHECK(forwarded==before);
 p=sample(1,1,PACKET_DESTINATION_BROADCAST,2,1);p.buffer[p.dataLength-2]=1;packet_receive(&p);CHECK(accepted==1&&forwarded==before);
 p=sample(1,1,PACKET_DESTINATION_BROADCAST,1,1);packet_receive(&p);CHECK(accepted==1&&forwarded==before);
 p=sample(1,1,PACKET_DESTINATION_BROADCAST,2,0);packet_receive(&p);CHECK(accepted==2&&objects[1].oIntangibleTimer==0);
 CHECK(!character_net_interaction_state(1,&contact,NULL));
 /* Server-owned directed host pose is legitimate at the client. */
 character_net_clear_all();gNetworkType=NT_CLIENT;
 gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;gNetworkPlayerServer=&gNetworkPlayers[1];
 p=sample(1,0,1,1,1);packet_receive(&p);CHECK(accepted==3&&objects[1].oIntangibleTimer==-1);
 p=sample(2,2,PACKET_DESTINATION_BROADCAST,3,1);packet_receive(&p);CHECK(accepted==3);
 p=sample(1,0,2,3,1);packet_receive(&p);CHECK(accepted==3);
 /* Accepted native presentation is rendered remotely, never an impact source. */
 gNetworkPlayers[1].currPositionValid=true;gNetworkPlayers[1].currLevelSyncValid=true;gNetworkPlayers[1].currAreaSyncValid=true;
 gNetworkPlayerLocal->currLevelSyncValid=true;gNetworkPlayerLocal->currAreaSyncValid=true;
 CharacterNetState state={0};CHECK(character_net_interaction_snapshot(1,&state));
 CHECK(state.active==CNET_DRIVING);
 fixtureNow+=.251;CHECK(!character_net_interaction_snapshot(1,&state));
 p=sample(1,0,1,4,CNET_PRESENTATION);packet_receive(&p);CHECK(accepted==4);
 CHECK(!character_net_interaction_snapshot(1,&state));
 CHECK(character_net_remote_update(&gMarioStates[1]));
 CHECK(objects[1].header.gfx.node.flags&GRAPH_RENDER_INVISIBLE);
 drawCalls=0;character_net_draw(NULL,NULL,NULL);CHECK(drawCalls==1);
 /* The real outbound writer preserves render-only activity too. */
 gCLIOpts.rocketCar=true;presentationSnapshot=&drawnSnapshot;
 struct Packet out={0};CHECK(character_net_write(&out));
 CHECK(character_net_decode(&state,out.buffer,CNET_WIRE_SIZE));CHECK(state.active==CNET_PRESENTATION&&!state.interaction);
 presentationSnapshot=NULL;
 p=sample(1,0,1,5,CNET_DRIVING);packet_receive(&p);CHECK(character_net_interaction_snapshot(1,&state));
 gMarioStates[1].health=0xff;CHECK(!character_net_interaction_snapshot(1,&state));gMarioStates[1].health=0x880;
 gMarioStates[1].freeze=1;CHECK(!character_net_interaction_snapshot(1,&state));gMarioStates[1].freeze=0;
 gMarioStates[1].heldObj=&objects[2];CHECK(!character_net_interaction_snapshot(1,&state));gMarioStates[1].heldObj=NULL;
 gNetworkPlayerLocal->currAreaSyncValid=false;CHECK(!character_net_interaction_snapshot(1,&state));gNetworkPlayerLocal->currAreaSyncValid=true;
 gNetworkPlayers[1].currLevelAreaSeqId++;CHECK(!character_net_interaction_snapshot(1,&state));
 gNetworkPlayers[1].currLevelAreaSeqId--;
 uint32_t oldGeneration,newGeneration;
 CHECK(character_net_interaction_state(1,&state,&oldGeneration));
 character_net_clear(1);CHECK(!character_net_interaction_snapshot(1,&state));
 p=sample(1,0,1,1,CNET_DRIVING);packet_receive(&p);
 CHECK(character_net_interaction_state(1,&state,&newGeneration)&&newGeneration!=oldGeneration);
 /* Jet mode comes from the same accepted native action/cap packet as motion.
  * It cannot affect the local tank or be replayed over a newer surface exit. */
 int beforeWater=accepted;
 p=sample(1,0,1,4,1);
 struct PacketPlayerData water={0};water.action=ACT_WATER_IDLE;water.health=0x800;
 water.levelSyncValid=water.areaSyncValid=1;
 size_t nativeOffset=p.dataLength-CNET_WIRE_SIZE-sizeof water;
 memcpy(p.buffer+nativeOffset,&water,sizeof water);packet_receive(&p);
 CHECK(accepted==beforeWater+1&&character_net_water_mode(1)==ROCKET_WATER_JET);
 struct Packet oldWater=p;oldWater.cursor=3;
 p=sample(1,0,1,5,1);water.action=ACT_METAL_WATER_FALLING;water.flags=MARIO_METAL_CAP|MARIO_WING_CAP;verifiedCapFlags[1]=water.flags;
 memcpy(p.buffer+nativeOffset,&water,sizeof water);packet_receive(&p);
 CHECK(character_net_water_mode(1)==ROCKET_WATER_METAL);
 packet_receive(&oldWater);CHECK(character_net_water_mode(1)==ROCKET_WATER_METAL&&accepted==beforeWater+2);
 verifiedCapFlags[1]=0;p=sample(1,0,1,6,1);packet_receive(&p);CHECK(character_net_water_mode(1)==ROCKET_WATER_DRY);
 p=sample(1,0,1,7,1);water.action=ACT_WATER_IDLE;water.flags=0;
 memcpy(p.buffer+nativeOffset,&water,sizeof water);packet_receive(&p);CHECK(character_net_water_mode(1)==ROCKET_WATER_JET);
 fixtureNow+=1.01;CHECK(character_net_water_mode(1)==ROCKET_WATER_DRY);
 character_net_clear(1);CHECK(character_net_water_mode(1)==ROCKET_WATER_DRY);
 p=sample(1,0,1,1,1);memcpy(p.buffer+nativeOffset,&water,sizeof water);packet_receive(&p);
 CHECK(character_net_water_mode(1)==ROCKET_WATER_JET); // new connection starts a fresh sequence
 gNetworkPlayers[1].connected=false;CHECK(character_net_water_mode(1)==ROCKET_WATER_DRY);
 gNetworkPlayers[1].connected=true;character_net_clear_all();CHECK(character_net_water_mode(1)==ROCKET_WATER_DRY);
 CHECK(gMarioStates[0].action==0&&gMarioStates[0].flags==0);
 /* Native authoritative flags reach the remote material without granting any
  * local powers. Replay/inactive/disconnect gates remain the native ingress. */
 character_net_clear_all();gNetworkType=NT_SERVER;
 gNetworkPlayers[0].globalIndex=0;gNetworkPlayers[1].globalIndex=1;gNetworkPlayerServer=gNetworkPlayerLocal;
 gNetworkPlayers[1].currPositionValid=gNetworkPlayers[1].currLevelSyncValid=gNetworkPlayers[1].currAreaSyncValid=true;
 float view[16]={0},projection[16]={0};int viewport[4]={0,0,64,64};
 packetCaps=MARIO_METAL_CAP|MARIO_WING_CAP|MARIO_VANISH_CAP;verifiedCapFlags[1]=packetCaps;
 p=sample(1,1,PACKET_DESTINATION_BROADCAST,20,1);packet_receive(&p);
 character_net_draw(view,projection,viewport);CHECK(drawCalls&&(drawnCaps&MARIO_SPECIAL_CAPS)==packetCaps);
 CHECK(!gMarioStates[0].flags); // Another owner's caps never grant local protection.
 packetCaps=0;verifiedCapFlags[1]=0;p=sample(1,1,PACKET_DESTINATION_BROADCAST,21,1);packet_receive(&p);
 character_net_draw(view,projection,viewport);CHECK(!drawnCaps);
 packetCaps=MARIO_METAL_CAP;p=sample(1,1,PACKET_DESTINATION_BROADCAST,20,1);packet_receive(&p);
 character_net_draw(view,projection,viewport);CHECK(!drawnCaps); // Stale pre-expiry packet rejected.
 // An authenticated owner's raw cap bit is still not an earned lease.
 p=sample(1,1,PACKET_DESTINATION_BROADCAST,22,1);packet_receive(&p);
 character_net_draw(view,projection,viewport);CHECK(!(drawnCaps&MARIO_SPECIAL_CAPS));
 CHECK(!(gMarioStates[1].flags&MARIO_SPECIAL_CAPS));
 p=sample(1,1,PACKET_DESTINATION_BROADCAST,23,1);water.action=ACT_WATER_IDLE;water.flags=MARIO_METAL_CAP;
 memcpy(p.buffer+nativeOffset,&water,sizeof water);packet_receive(&p);
 CHECK(character_net_water_mode(1)==ROCKET_WATER_JET); // Forged Metal cannot suppress the remote jet mode.
 unsigned priorDraws=drawCalls;gNetworkPlayers[1].connected=false;character_net_draw(view,projection,viewport);CHECK(drawCalls==priorDraws);
 gNetworkPlayers[1].connected=true;character_net_clear(1);character_net_draw(view,projection,viewport);CHECK(drawCalls==priorDraws);
 packetCaps=0;p=sample(1,1,PACKET_DESTINATION_BROADCAST,1,0);packet_receive(&p);
 character_net_draw(view,projection,viewport);CHECK(drawCalls==priorDraws);
 test_native_caps();
 test_online_switch_transport();
 printf("character transport: %d checks passed (includes native cap remote material, expiry/replay, disconnect/slot reset)\n",checks);return 0;
}
