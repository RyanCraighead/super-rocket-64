/* Actual player packet ingress + CNET + roof-carry identity/render predicates.
 * Transport is in memory. No wire format changes, sockets or gameplay input. */
#define ROCKET_PENGUIN_REAL_TEST
#define main transport_regression_main
#include "test_character_transport.c"
#undef main
#include "game/rocket_penguin.h"
#include "behavior_data.h"
const BehaviorScript bhvSmallPenguin[]={1},bhvPenguinBaby[]={2},bhvUnused20E0[]={3};
static struct Object baby;
static struct SyncObject babySync;
static int drops;
int rocket_adapter_car_selected(void){return 0;}
struct SyncObject *sync_object_get(u32 id){return id==17?&babySync:NULL;}
void mario_drop_held_object(struct MarioState *m){drops++;m->heldObj=NULL;}
static struct Packet held_packet(unsigned sequence,unsigned syncID){
    struct Packet p=sample(1,0,1,sequence,CNET_DRIVING);
    struct PacketPlayerData data={0};data.action=ACT_IDLE;data.health=0x880;
    data.levelSyncValid=data.areaSyncValid=1;data.heldSyncID=syncID;
    memcpy(p.buffer+p.dataLength-CNET_WIRE_SIZE-sizeof data,&data,sizeof data);
    return p;
}
int main(void){
    transport_regression_main();character_net_clear_all();
    memset(gMarioStates,0,sizeof gMarioStates);memset(gNetworkPlayers,0,sizeof gNetworkPlayers);
    gCLIOpts.characterNet=1;gNetworkType=NT_CLIENT;gNetworkAreaLoaded=1;
    gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerServer=&gNetworkPlayers[1];
    for(int i=0;i<2;i++){
        gNetworkPlayers[i].globalIndex=!i;gNetworkPlayers[i].localIndex=i;gNetworkPlayers[i].connected=1;
        gNetworkPlayers[i].currPositionValid=gNetworkPlayers[i].currAreaSyncValid=gNetworkPlayers[i].currLevelSyncValid=1;
        gMarioStates[i].playerIndex=i;gMarioStates[i].marioObj=&objects[i];gMarioStates[i].controller=&controllers[i];
        gMarioStates[i].health=0x880;
    }
    baby.behavior=bhvSmallPenguin;baby.activeFlags=ACTIVE_FLAG_ACTIVE;baby.oSyncID=17;babySync.o=&baby;
    struct Packet p=held_packet(1,17);packet_receive(&p);
    CHECK(gMarioStates[1].heldObj==&baby&&baby.oHeldState==HELD_HELD&&baby.heldByPlayerIndex==1);
    CHECK(rocket_penguin_carried(&gMarioStates[1]));
    CharacterNetState contact;CHECK(character_net_interaction_state(1,&contact,NULL));
    baby.header.gfx.node.flags=0;rocket_penguin_render_held(&baby);
    CHECK(baby.header.gfx.node.flags&GRAPH_RENDER_ACTIVE);
    CHECK(baby.oPosX==-20&&baby.oPosZ==80); // Packet basis is identity; native Y-up conversion is preserved.
    struct Packet replay=p;replay.cursor=3;
    p=held_packet(2,0);packet_receive(&p);CHECK(!gMarioStates[1].heldObj);
    packet_receive(&replay);CHECK(!gMarioStates[1].heldObj);
    p=held_packet(3,17);gMarioStates[0].heldObj=&baby;packet_receive(&p);
    CHECK(drops==1&&!gMarioStates[0].heldObj&&gMarioStates[1].heldObj==&baby); // Native global-index tie break.
    fixtureNow+=.251;CHECK(!character_net_interaction_state(1,&contact,NULL));
    fixtureNow+=1.;baby.header.gfx.node.flags=0;rocket_penguin_render_held(&baby);CHECK(!baby.header.gfx.node.flags);
    p=held_packet(4,17);packet_receive(&p);CHECK(character_net_interaction_state(1,&contact,NULL));
    gNetworkPlayers[1].currAreaIndex++;CHECK(!character_net_interaction_state(1,&contact,NULL));
    gNetworkPlayers[1].currAreaIndex--;baby.behavior=NULL;CHECK(!character_net_interaction_state(1,&contact,NULL));
    baby.behavior=bhvSmallPenguin;character_net_clear(1);CHECK(!rocket_penguin_carried(&gMarioStates[1]));
    printf("PASS penguin native transport and CNET: %d checks including existing ingress regressions\n",checks);
    return 0;
}
