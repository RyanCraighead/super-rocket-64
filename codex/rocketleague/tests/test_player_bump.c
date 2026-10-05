#include "speed_fixture_stubs.h"
/* Real authority/contact/packet logic; explicit headless transport and native
 * movement services. Three independently saved sessions model host and clients. */
#include "../../../src/pc/player_bump.c"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"bump line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
struct CLIOptions gCLIOpts;
enum NetworkType gNetworkType;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal,*gNetworkPlayerServer;
struct MarioState gMarioStates[MAX_PLAYERS];
struct ServerSettings gServerSettings;
bool gNetworkAreaLoaded,gNetworkAreaSyncing;
struct Area *gCurrentArea;
struct WarpTransition gWarpTransition;
struct WarpDest sWarpDest;
s16 sCurrPlayMode,sDelayedWarpOp,gCurrCourseNum,gCurrActStarNum,gCurrLevelNum,gCurrAreaIndex;
u32 gGlobalTimer,gTimeStopState;
static struct Area area;
static struct Object objects[MAX_PLAYERS];
static struct BumpSession sessions[3];
static RocketSnapshot cars[3];
static struct MarioState native[3];
static u32 epochs[3],sequences[3],caps[3];
static u16 areaIds[3];
static unsigned kinds[3],applied[3],active;
static int connected[3],wall,hostArea,areaNumber[3];
static double now;
struct Message {unsigned from,to;struct Packet packet;};
static struct Message messages[1024];static unsigned messageCount;
static unsigned global(unsigned local){return local?((local<=active)?local-1:local):active;}
static unsigned local(unsigned id){return id==active?0:id<active?id+1:id;}
double clock_elapsed_f64(void){return now;}
uint32_t rocket_runtime_epoch(void){return epochs[active];}
unsigned character_net_local_kind(void){return kinds[active];}
int character_net_is_car(unsigned i){return i<3&&kinds[global(i)]==CNET_OCTANE;}
int rocket_adapter_car_selected(void){return kinds[active]==CNET_OCTANE;}
int rocket_adapter_body_snapshot(struct Object *o,RocketSnapshot *s){CHECK(o==gMarioStates[0].marioObj);*s=cars[active];return 1;}
int rocket_adapter_whomp_path_clear(const float a[3],const float b[3],struct Object *o){(void)a;(void)b;CHECK(!o);return !wall;}
u32 rocket_caps_active_flags(unsigned i){return i<3?caps[global(i)]:0;}
int rocket_runtime_bump(const float d[3]){for(int k=0;k<3;k++)cars[active].velocity[k]+=d[k];applied[active]++;return 1;}
u32 set_mario_action(struct MarioState *m,u32 action,u32 arg){m->action=action;m->actionArg=arg;return 1;}
void mario_set_forward_vel(struct MarioState *m,float speed){m->forwardVel=speed;m->vel[0]=sins(m->faceAngle[1])*speed;m->vel[2]=coss(m->faceAngle[1])*speed;}
struct NetworkPlayer *network_player_from_global_index(u8 id){for(int i=0;i<3;i++)if(gNetworkPlayers[i].connected&&gNetworkPlayers[i].globalIndex==id)return &gNetworkPlayers[i];return NULL;}
void network_send_to(u8 to,struct Packet *p){
    CHECK(messageCount<1024&&to<3&&to>0&&p->dataLength==9+BUMP_WIRE);
    struct Message *m=&messages[messageCount++];m->from=active;m->to=global(to);m->packet=*p;
    packet_set_destination(&m->packet,m->to);
}
static void select_peer(unsigned id){
    sessions[active]=bumps;native[active]=gMarioStates[0];active=id;bumps=sessions[id];
    memset(gNetworkPlayers,0,sizeof gNetworkPlayers);memset(gMarioStates,0,sizeof gMarioStates);
    for(unsigned i=0;i<3;i++){
        unsigned who=global(i);struct NetworkPlayer *n=&gNetworkPlayers[i];
        n->connected=connected[who];n->localIndex=i;n->globalIndex=who;
        n->currPositionValid=n->currLevelSyncValid=n->currAreaSyncValid=true;
        n->currCourseNum=1;n->currActNum=1;n->currLevelNum=9;n->currAreaIndex=areaNumber[who];n->currLevelAreaSeqId=areaIds[who];
        gMarioStates[i]=native[who];gMarioStates[i].playerIndex=i;gMarioStates[i].marioObj=&objects[i];gMarioStates[i].area=&area;
        memcpy(gMarioStates[i].pos,cars[who].position,sizeof(Vec3f));
        for(int k=0;k<3;k++)gMarioStates[i].vel[k]=cars[who].velocity[k]/30.f;
    }
    gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerServer=&gNetworkPlayers[local(0)];
    gNetworkType=id?NT_CLIENT:NT_SERVER;gCurrAreaIndex=areaNumber[id];area.index=gCurrAreaIndex;
}
static void publish(void){
    unsigned saved=active;for(int i=0;i<3;i++)sequences[i]++;
    for(unsigned peer=0;peer<3;peer++){
        select_peer(peer);
        for(unsigned source=0;source<3;source++){
            CharacterNetState state={0};state.speed_percent=rocket_speed_percent();state.rule_revision=rocket_rule_revision();state.kind=kinds[source];state.active=state.kind==CNET_OCTANE?CNET_DRIVING:0;
            state.epoch=epochs[source];state.sequence=sequences[source];state.area_sequence=areaIds[source];state.car=cars[source];
            float velocity[3];for(int k=0;k<3;k++)velocity[k]=cars[source].velocity[k]/30.f;
            player_bump_observe(local(source),&state,cars[source].position,velocity);
        }
    }
    select_peer(saved);
}
static void fresh(void){
    fixtureSpeedPercent=100;fixtureRuleRevision=0;
    memset(&bumps,0,sizeof bumps);memset(sessions,0,sizeof sessions);memset(native,0,sizeof native);memset(gMarioStates,0,sizeof gMarioStates);
    memset(cars,0,sizeof cars);memset(caps,0,sizeof caps);memset(applied,0,sizeof applied);memset(sequences,0,sizeof sequences);
    memset(&gWarpTransition,0,sizeof gWarpTransition);memset(&sWarpDest,0,sizeof sWarpDest);sDelayedWarpOp=gTimeStopState=0;
    sCurrPlayMode=PLAY_MODE_NORMAL;gCLIOpts.characterNet=true;gCLIOpts.offline=false;
    gNetworkAreaLoaded=true;gNetworkAreaSyncing=false;gServerSettings.playerInteractions=PLAYER_INTERACTIONS_SOLID;
    gCurrentArea=&area;area.localAreaTimer=100;gCurrCourseNum=gCurrActStarNum=1;gCurrLevelNum=9;
    gGlobalTimer=100;now=10;wall=hostArea=0;messageCount=0;active=0;
    for(int i=0;i<3;i++){
        cars[i].basis[0]=cars[i].basis[7]=1;cars[i].basis[5]=-1;cars[i].position[1]=40;
        cars[i].position[0]=i==2?4000:i*230;cars[i].boost=40;cars[i].ticks=100;
        native[i].health=0x880;native[i].action=ACT_IDLE;native[i].numStars=70;native[i].numCoins=50;
        epochs[i]=1;areaIds[i]=100+i;kinds[i]=CNET_OCTANE;connected[i]=1;areaNumber[i]=1;
    }
    gMarioStates[0]=native[0];select_peer(0);publish();
}
static void tick(unsigned peer){select_peer(peer);player_bump_update();}
static struct Packet received(struct Message m){
    select_peer(m.to);struct Packet p=m.packet;p.localIndex=local(m.from);p.cursor=3;p.error=false;
    CHECK(packet_initial_read(&p));return p;
}
static void deliver(struct Message m){struct Packet p=received(m);player_bump_receive(&p);}
static void unchanged(void){for(int i=0;i<3;i++)CHECK(native[i].health==0x880&&native[i].numStars==70&&native[i].numCoins==50&&cars[i].boost==40&&cars[i].ticks==100);}
static void impact(float latency){
    fresh();cars[0].velocity[0]=1200;publish();tick(0);
    CHECK(applied[0]==1&&applied[1]==0&&messageCount==1);CHECK(cars[0].velocity[0]<600);
    struct Message grant=messages[0];float hostVelocity=cars[0].velocity[0];
    for(int i=0;i<5;i++){gGlobalTimer++;now+=.01;tick(0);CHECK(applied[0]==1);}
    now+=latency;deliver(grant);CHECK(applied[1]==1&&cars[1].velocity[0]>600);
    float clientVelocity=cars[1].velocity[0];CHECK(fabsf(hostVelocity+clientVelocity-1200)<.001f);
    deliver(grant);CHECK(applied[1]==1&&cars[1].velocity[0]==clientVelocity);
    CHECK(messageCount==3);deliver(messages[2]);deliver(messages[1]);
    select_peer(0);CHECK(bumps.pairs[0][1].acks==3);gGlobalTimer+=4;publish();tick(0);CHECK(applied[0]==1);
    unchanged();
}
#define REJECT(change) do{fresh();cars[0].velocity[0]=1200;publish();change;tick(0);CHECK(applied[0]==0&&messageCount==0);}while(0)
int main(void){
    impact(0);impact(.05f);impact(.1f);
    REJECT(gServerSettings.playerInteractions=PLAYER_INTERACTIONS_NONE);
    REJECT(gNetworkAreaSyncing=true);REJECT(gNetworkAreaLoaded=false);REJECT(gCLIOpts.offline=true);
    REJECT(wall=1);REJECT(caps[0]=MARIO_VANISH_CAP);REJECT(caps[1]=MARIO_VANISH_CAP);
    REJECT(gMarioStates[0].health=0xff);REJECT(native[1].freeze=1);REJECT(gMarioStates[0].action=ACT_DISAPPEARED);
    REJECT(native[1].action=ACT_EMERGE_FROM_PIPE);REJECT(gMarioStates[0].skipWarpInteractionsTimer=30);
    REJECT(gWarpTransition.isActive=true);REJECT(sCurrPlayMode=PLAY_MODE_PAUSED);
    REJECT(areaNumber[1]=2);REJECT(connected[1]=0);REJECT(now+=.21);
    REJECT(kinds[0]=kinds[1]=CNET_MARIO;publish());
    /* A non-authority cannot generate grants, even when it owns the moving car. */
    fresh();cars[1].velocity[0]=-1200;publish();tick(1);CHECK(!messageCount&&!applied[1]);tick(0);CHECK(messageCount==1&&applied[0]==1);
    /* Replay, malformed sender/route/size/value and delayed lifecycle rejection. */
    fresh();cars[0].velocity[0]=1200;publish();tick(0);struct Message grant=messages[0];
    struct Packet p=received(grant);CHECK(player_bump_packet_allowed(&p));
    p.localIndex=local(2);CHECK(!player_bump_packet_allowed(&p));
    p=received(grant);p.dataLength--;CHECK(!player_bump_packet_allowed(&p));
    p=received(grant);p.requestBroadcast=true;CHECK(!player_bump_packet_allowed(&p));
    p=received(grant);p.destGlobalId=2;CHECK(!player_bump_packet_allowed(&p));
    p=received(grant);float bad=NAN;memcpy(p.buffer+p.dataLength-8,&bad,4);CHECK(!player_bump_packet_allowed(&p));
    epochs[1]++;deliver(grant);CHECK(!applied[1]);epochs[1]--;deliver(grant);CHECK(!applied[1]);
    fresh();cars[0].velocity[0]=1200;publish();tick(0);grant=messages[0];now+=.41;deliver(grant);CHECK(!applied[1]);
    fresh();cars[0].velocity[0]=1200;publish();tick(0);grant=messages[0];select_peer(1);player_bump_clear(0);deliver(grant);CHECK(!applied[1]);
    fresh();cars[0].velocity[0]=1200;publish();tick(0);grant=messages[0];areaIds[1]++;deliver(grant);CHECK(!applied[1]);
    fresh();cars[0].velocity[0]=1200;publish();tick(0);grant=messages[0];kinds[1]=CNET_MARIO;deliver(grant);CHECK(!applied[1]);
    /* No-input stationary penetration separates gently, without any damage. */
    fresh();tick(0);CHECK(applied[0]==1&&fabsf(cars[0].velocity[0])<60);deliver(messages[0]);CHECK(applied[1]==1);unchanged();
    /* Fast translational crossing at native send intervals is swept; a warp is not. */
    fresh();cars[1].position[0]=500;cars[0].velocity[0]=cars[1].velocity[0]=0;publish();
    now+=.1;cars[0].velocity[0]=4600;cars[0].position[0]=700;publish();tick(0);CHECK(applied[0]==1&&messageCount==1);
    fresh();cars[1].position[0]=500;publish();now+=.1;cars[0].position[0]=700;cars[0].velocity[0]=4600;epochs[0]++;publish();tick(0);CHECK(!applied[0]&&!messageCount);
    /* Native Mario retains ordinary movement ownership; car contact adds no health damage. */
    fresh();kinds[1]=CNET_MARIO;cars[1].position[0]=170;cars[1].position[1]=0;cars[0].velocity[0]=1200;publish();tick(0);CHECK(messageCount==1);deliver(messages[0]);CHECK(gMarioStates[0].health==0x880&&gMarioStates[0].action==ACT_SOFT_BACKWARD_GROUND_KB);
    /* Area authority migrates to the lowest local participant when the server
     * is elsewhere; its own local simulation remains untouched. */
    fresh();areaNumber[0]=2;cars[0].position[0]=4000;cars[1].position[0]=0;cars[2].position[0]=230;
    cars[1].velocity[0]=1200;publish();tick(1);CHECK(applied[1]==1&&messageCount==1);deliver(messages[0]);CHECK(applied[2]==1&&!applied[0]);
    /* Slot reuse retires old references; fresh owner poses permit new contact. */
    fresh();cars[0].velocity[0]=1200;publish();tick(0);grant=messages[0];deliver(grant);CHECK(applied[1]==1);
    select_peer(1);player_bump_clear(local(0));deliver(grant);CHECK(applied[1]==1);
    cars[0].velocity[0]=1200;cars[1].velocity[0]=0;now+=.1;gGlobalTimer+=4;
    select_peer(0);player_bump_clear(local(1));publish();tick(0);grant=messages[messageCount-1];deliver(grant);CHECK(applied[1]==2);
    /* A host rule transition retires pending grants and both old pose directions. */
    fresh();cars[0].velocity[0]=1200;publish();tick(0);grant=messages[0];
    fixtureSpeedPercent=75;fixtureRuleRevision=1;deliver(grant);CHECK(!applied[1]);
    fixtureSpeedPercent=100;fixtureRuleRevision=2;deliver(grant);CHECK(!applied[1]);
    // New owner poses authorize scaled impulses; no old history is reused.
    fresh();fixtureSpeedPercent=75;fixtureRuleRevision=1;
    for(unsigned i=0;i<3;i++){select_peer(i);player_bump_update();}
    cars[0].velocity[0]=900;publish();gGlobalTimer++;tick(0);
    CHECK(messageCount==1&&applied[0]==1);grant=messages[0];deliver(grant);CHECK(applied[1]==1);
    CHECK(cars[1].velocity[0]>450&&cars[1].velocity[0]<600);unchanged();
    printf("PASS %u player-bump authority/contact/packet checks; explicit transport/native services, no live game\n",checks);
}
