/* Real lease service, native topper, player transport and packet ingress.
 * The fixture supplies inert world/audio services: never opens a window. */
#define ROCKET_WING_REAL_TEST
#include "character_transport_fixture.h"
#include "../../../src/game/rocket_wing.h"
#include "../../../src/game/object_helpers.h"
#include "../../../src/game/sound_init.h"
#include "../../../include/behavior_data.h"
#include "../../../include/behavior_table.h"
#include "../../../include/model_ids.h"
#include "../../../include/course_table.h"
#include "../../../include/level_table.h"
#include "../physics/body_contact.h"
const BehaviorScript bhvWingCap[]={1}, bhvExclamationBox[]={2}, bhvStaticObject[]={3}, bhvMetalCap[]={4}, bhvVanishCap[]={5};
static struct ObjectNode lists[NUM_OBJ_LISTS];
struct ObjectNode *gObjectLists=lists;
static struct Area fixtureArea;
static struct Object cap, box, visual[MAX_PLAYERS];
static struct SyncObject boxSync;
static unsigned collected, sounds, stopped, boxSent;
static u64 session=1234;
static int ordinaryMode;
u32 gGlobalTimer;
u8 sSquishScaleOverTime[16]={46,71,85,95,100,98,91,80,66,50,34,20,9,2,0,0};
static u32 attack=INT_HIT_FROM_BELOW;
static int localPickupKind,blockedContact,boxContact=1;
static u32 contactCaps;
int rocket_adapter_pickup_pose(RocketSnapshot *out){
    if(localPickupKind>0){if(!sourceSnapshot)return -1;*out=*sourceSnapshot;}
    return localPickupKind;
}
/* Explicit geometry-service fixture. Production adapter tests cover actual
 * surfaces/visibility; these checks prove authority uses its result/raw pose. */
int rocket_adapter_cap_pickup_contact(const RocketSnapshot *car,struct Object *o,unsigned flags){
    contactCaps=flags;
    if(blockedContact||!(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->oIntangibleTimer||
       (o->oInteractStatus&INT_STATUS_INTERACTED)||o->header.gfx.activeAreaIndex!=1)return 0;
    float bottom[]={o->oPosX,o->oPosY-o->hitboxDownOffset,o->oPosZ};
    return rocket_body_overlaps_cylinder(car,bottom,o->hitboxRadius,o->hitboxHeight);
}
int rocket_adapter_cap_box_pose_contact(const RocketSnapshot *car,struct Object *o,unsigned flags){
    return boxContact&&o->oAction==2&&o->oBehParams2ndByte<=2&&
        rocket_adapter_cap_pickup_contact(car,o,flags);
}
u32 determine_interaction(struct MarioState *m,struct Object *o){(void)m;(void)o;return attack;}
int rocket_boost_mode(void){return ordinaryMode;}
uint64_t rocket_boost_session_id(void){return session;}
void play_cap_music(u16 s){(void)s;sounds++;}
void stop_cap_music(void){stopped++;}
void fadeout_cap_music(void){}
void network_send_collect_item(struct Object *o){CHECK(o==&cap);collected++;}
void network_send_object(struct Object *o){CHECK(o==&box);boxSent++;}
struct SyncObject *sync_object_get(u32 i){return i==55?&boxSync:NULL;}
const BehaviorScript *get_behavior_from_id(enum BehaviorId i){return i==1?bhvWingCap:i==2?bhvExclamationBox:i==4?bhvMetalCap:i==5?bhvVanishCap:bhvStaticObject;}
static s32 topperModel;
void obj_set_model(struct Object *o,s32 model){(void)o;topperModel=model;}
void obj_mark_for_deletion(struct Object *o){o->activeFlags=ACTIVE_FLAG_DEACTIVATED;}
struct Object *spawn_object(struct Object *parent,s32 model,const BehaviorScript *behavior){
    CHECK(model==MODEL_MARIOS_WING_CAP&&behavior==bhvStaticObject);
    unsigned i=(unsigned)(parent-objects);CHECK(i<MAX_PLAYERS);
    struct Object *o=&visual[i];memset(o,0,sizeof *o);o->activeFlags=ACTIVE_FLAG_ACTIVE;o->behavior=behavior;return o;
}
static bool match(void *a,void *b){return a==b;}
static void fresh(void){
    rocket_caps_clear_all();character_net_clear_all();
    memset(gMarioStates,0,sizeof gMarioStates);memset(gNetworkPlayers,0,sizeof gNetworkPlayers);
    memset(&cap,0,sizeof cap);memset(&box,0,sizeof box);memset(visual,0,sizeof visual);
    fixtureArea.index=1;gNetworkAreaLoaded=true;gCLIOpts.characterNet=true;gCLIOpts.offline=false;
    gCurrAreaIndex=1;gCurrCourseNum=gCurrLevelNum=gCurrActStarNum=0;
    gNetworkType=NT_SERVER;gNetworkPlayerServer=gNetworkPlayerLocal=&gNetworkPlayers[0];
    gNetworkServerAddr=NULL;fixtureNow+=2;collected=0;
    sourceSnapshot=presentationSnapshot=NULL;localPickupKind=blockedContact=0;boxContact=1;contactCaps=0;
    for(unsigned i=0;i<NUM_OBJ_LISTS;i++)lists[i].next=lists[i].prev=&lists[i];
    for(unsigned i=0;i<2;i++){
        struct NetworkPlayer *np=&gNetworkPlayers[i];np->connected=true;np->localIndex=i;np->globalIndex=i;
        np->currAreaSyncValid=np->currLevelSyncValid=np->currPositionValid=true;np->currAreaIndex=1;np->currLevelAreaSeqId=5;
        struct MarioState *m=&gMarioStates[i];m->playerIndex=i;m->marioObj=&objects[i];m->area=&fixtureArea;
        m->health=0x880;m->action=ACT_IDLE;m->flags=MARIO_NORMAL_CAP|MARIO_CAP_ON_HEAD;
        m->pos[0]=i?0:1000;objects[i].globalPlayerIndex=i;
    }
    cap.behavior=bhvWingCap;cap.activeFlags=ACTIVE_FLAG_ACTIVE;cap.hitboxRadius=80;cap.hitboxHeight=80;
    cap.header.gfx.activeAreaIndex=1;cap.header.next=cap.header.prev=&lists[OBJ_LIST_LEVEL];
    lists[OBJ_LIST_LEVEL].next=lists[OBJ_LIST_LEVEL].prev=&cap.header;
    gLevelValues.wingCapDuration=gLevelValues.metalCapDuration=gLevelValues.vanishCapDuration=90;
    CharacterNetState car={0};car.speed_percent=100;car.sequence=1;car.kind=CNET_MARIO;car.area_sequence=5;
    CHECK(character_net_accept(1,&car));
    boxSync.o=&box;box.behavior=bhvExclamationBox;box.activeFlags=ACTIVE_FLAG_ACTIVE;
    box.header.gfx.activeAreaIndex=1;box.oAction=2;box.hitboxRadius=40;box.hitboxHeight=30;
}
static struct Packet pickup(void){
    struct Packet p={0};capturedPacket=&p;rocket_caps_update();capturedPacket=NULL;
    CHECK(p.packetType==PACKET_ROCKET_CAP_STATE&&p.reliable);CHECK(collected==1);
    CHECK(rocket_wing_active(1));CHECK(gMarioStates[1].capTimer==90);return p;
}
static void receive(struct Packet p){p.localIndex=1;p.addr=(void*)1;p.cursor=3;packet_receive(&p);}
static void as_client(void){
    rocket_caps_clear_all();gNetworkType=NT_CLIENT;gNetworkPlayerServer=&gNetworkPlayers[1];gNetworkPlayerLocal=&gNetworkPlayers[0];
    gNetworkPlayers[0].globalIndex=1;gNetworkPlayers[1].globalIndex=0;gNetworkServerAddr=(void*)1;
}
static CharacterNetState cap_car(unsigned sequence,float x){
    CharacterNetState car={0};car.speed_percent=100;car.sequence=sequence;car.epoch=1;car.area_sequence=5;
    car.kind=CNET_OCTANE;car.active=CNET_DRIVING;car.interaction=1;
    car.car.position[0]=x;car.car.basis[2]=car.car.basis[3]=car.car.basis[7]=1;
    for(int i=0;i<4;i++)car.car.wheel_radius[i]=32;
    return car;
}
static struct Packet box_request(void){
    struct Packet p={0};u8 owner=1;u32 sync=55,behavior=2;u16 event=1,seed=0;
    packet_init(&p,PACKET_OBJECT,true,PLMT_AREA);packet_write(&p,&owner,1);packet_write(&p,&sync,4);
    packet_write(&p,&event,2);packet_write(&p,&seed,2);packet_write(&p,&behavior,4);
    p.localIndex=1;p.cursor=10;p.destGlobalId=PACKET_DESTINATION_BROADCAST;return p;
}
static void chassis_authority_tests(void){
    const BehaviorScript *caps[]={bhvWingCap,bhvMetalCap,bhvVanishCap};
    const u32 flags[]={MARIO_WING_CAP,MARIO_METAL_CAP,MARIO_VANISH_CAP};
    for(unsigned type=0;type<3;type++)for(unsigned local=0;local<2;local++){
        fresh();cap.behavior=caps[type];cap.oPosX=90;cap.oPosY=40;cap.hitboxRadius=10;cap.hitboxHeight=30;
        CharacterNetState car=cap_car(2,0);unsigned owner=local?0:1;
        if(local){gMarioStates[0].pos[0]=0;gMarioStates[1].pos[0]=1000;localPickupKind=1;sourceSnapshot=&car.car;}
        else CHECK(character_net_accept(1,&car));
        // Only the side of the actual chassis reaches this cap; the native
        // radius 37 + cap radius 10 capsule does not.
        rocket_caps_update();CHECK(collected==1&&rocket_caps_active_flags(owner)==flags[type]);
        rocket_caps_update();CHECK(collected==1); // Reservation survives repeat scans.
    }
    fresh();gMarioStates[0].pos[0]=0;gMarioStates[1].pos[0]=1000;localPickupKind=-1;
    rocket_caps_update();CHECK(!collected); // Suspended selected car cannot use proxy capsule.
    fresh();CharacterNetState car=cap_car(2,0);car.active=CNET_PRESENTATION;car.interaction=0;
    CHECK(character_net_accept(1,&car));rocket_caps_update();CHECK(!collected);
    car.active=CNET_DRIVING;car.sequence++;CHECK(character_net_accept(1,&car));rocket_caps_update();CHECK(!collected);
    car.interaction=1;car.sequence++;CHECK(character_net_accept(1,&car));fixtureNow+=.201;
    rocket_caps_update();CHECK(!collected); // Still renderable, no longer a fresh contact.
    car.sequence++;CHECK(character_net_accept(1,&car));gMarioStates[1].freeze=1;rocket_caps_update();CHECK(!collected);
    gMarioStates[1].freeze=0;gMarioStates[1].heldObj=&box;rocket_caps_update();CHECK(!collected);
    gMarioStates[1].heldObj=NULL;blockedContact=1;rocket_caps_update();CHECK(!collected);
    blockedContact=0;car.area_sequence=6;car.sequence++;CHECK(character_net_accept(1,&car));rocket_caps_update();CHECK(!collected);
    character_net_clear(1);car.area_sequence=5;car.sequence=1;CHECK(character_net_accept(1,&car));
    rocket_caps_update();CHECK(collected==1);

    for(unsigned type=0;type<3;type++){
        fresh();cap.activeFlags=0;car=cap_car(2,0);CHECK(character_net_accept(1,&car));
        box.oBehParams2ndByte=type;box.oPosX=100;box.oPosY=40;
        struct Packet request=box_request();unsigned sent=boxSent;attack=0;
        boxContact=0;CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent);
        boxContact=1;blockedContact=1;CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent);
        blockedContact=0;box.oAction=1;CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent);
        box.oAction=2;box.oIntangibleTimer=-1;CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent);
        box.oIntangibleTimer=0;request.localIndex=2;CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent);
        request.localIndex=1;CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent+1&&box.oExclamationBoxForce);
        CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent+1); // Request replay.
        box.oExclamationBoxForce=0;car.active=CNET_PRESENTATION;car.interaction=0;car.sequence++;
        CHECK(character_net_accept(1,&car));CHECK(!rocket_caps_object_allowed(&request)&&boxSent==sent+1);
    }
    attack=INT_HIT_FROM_BELOW;

    // The actor's verified mask is passed to geometry, not the host's cap or
    // an arbitrary native incoming flag. The adapter independently filters walls.
    fresh();gMarioStates[0].pos[0]=0;gMarioStates[1].pos[0]=1000;
    cap.behavior=bhvVanishCap;CHECK(rocket_caps_interact(&gMarioStates[0],&cap));
    car=cap_car(2,0);CHECK(character_net_accept(1,&car));cap.behavior=bhvWingCap;cap.oInteractStatus=0;
    gMarioStates[0].pos[0]=1000;blockedContact=1;rocket_caps_update();CHECK(contactCaps==0&&collected==1);
    blockedContact=0;cap.behavior=bhvVanishCap;rocket_caps_update();CHECK(rocket_caps_active_flags(1)==MARIO_VANISH_CAP);
    cap.behavior=bhvWingCap;cap.oInteractStatus=0;rocket_caps_update();CHECK(contactCaps==MARIO_VANISH_CAP);

    // A native presentation action can keep the car/hat after physics yields.
    fresh();gCLIOpts.offline=true;gNetworkType=NT_NONE;
    gMarioStates[0].flags|=MARIO_SPECIAL_CAPS;gMarioStates[0].capTimer=90;
    car=cap_car(2,123);presentationSnapshot=&car.car;rocket_wing_topper_update();
    CHECK(visual[0].activeFlags&ACTIVE_FLAG_ACTIVE);CHECK(visual[0].oPosX==123);
    CHECK(topperModel==MODEL_MARIOS_WINGED_METAL_CAP&&visual[0].oOpacity==128);
    gMarioStates[0].capTimer=0;rocket_wing_topper_update();CHECK(visual[0].activeFlags==ACTIVE_FLAG_DEACTIVATED);
    presentationSnapshot=NULL;
}
int main(void){
    static struct NetworkSystem system={.get_id_str=id,.match_addr=match,.requireServerBroadcast=true};gNetworkSystem=&system;
    fresh();CHECK(!rocket_wing_active(1));
    gMarioStates[1].flags|=MARIO_WING_CAP;gMarioStates[1].capTimer=65535;
    CHECK(!rocket_wing_active(1));rocket_caps_apply(&gMarioStates[1]);CHECK(!(gMarioStates[1].flags&MARIO_WING_CAP));
    struct Packet grant=pickup();CHECK(gMarioStates[1].capTimer==90); /* forged native timer ignored */
    rocket_caps_update();CHECK(collected==1&&gMarioStates[1].capTimer==89);
    cap.oInteractStatus=INT_STATUS_INTERACTED;rocket_caps_update();CHECK(collected==1);
    gMarioStates[1].action=ACT_IN_CANNON;rocket_caps_update();CHECK(gMarioStates[1].capTimer==87); /* no remote pause cheat */
    int before=forwarded;struct Packet bad=grant;bad.cursor=3;bad.localIndex=1;bad.buffer[4]=2;
    packet_receive(&bad);CHECK(forwarded==before);bad=grant;bad.cursor=3;bad.localIndex=1;bad.buffer[3]|=2;
    packet_receive(&bad);CHECK(forwarded==before);
    for(int i=0;i<87;i++)rocket_caps_update();CHECK(!rocket_wing_active(1)&&gMarioStates[1].capTimer==0);
    CHECK(!(gMarioStates[1].flags&MARIO_WING_CAP));
    fresh();grant=pickup();as_client();receive(grant);CHECK(rocket_wing_active(0)&&gMarioStates[0].capTimer==90);
    rocket_caps_update();CHECK(gMarioStates[0].capTimer==89);receive(grant);CHECK(gMarioStates[0].capTimer==89);
    bad=grant;bad.buffer[13]+=1;receive(bad);CHECK(gMarioStates[0].capTimer==89); /* newer heartbeat cannot refill */
    bad=grant;bad.buffer[13]+=2;bad.buffer[5]^=1;receive(bad);CHECK(gMarioStates[0].capTimer==89); /* session */
    bad=grant;bad.buffer[13]+=3;bad.buffer[21]++;receive(bad);CHECK(gMarioStates[0].capTimer==89); /* area */
    bad=grant;bad.buffer[13]+=4;bad.dataLength--;receive(bad);CHECK(gMarioStates[0].capTimer==89);
    bad=grant;bad.buffer[13]+=5;bad.localIndex=2;bad.addr=(void*)2;bad.cursor=3;packet_receive(&bad);CHECK(gMarioStates[0].capTimer==89);
    for(int i=0;i<89;i++)rocket_caps_update();CHECK(!rocket_wing_active(0));
    bad=grant;bad.buffer[13]+=6;receive(bad);CHECK(!rocket_wing_active(0)); /* expired grant cannot restart */
    fresh();grant=pickup();as_client();gMarioStates[0].marioObj=NULL;receive(grant);CHECK(!rocket_wing_active(0));
    gMarioStates[0].marioObj=&objects[0];bad=grant;bad.buffer[13]+=1;receive(bad);CHECK(rocket_wing_active(0)); /* join readiness */
    fresh();cap.oIntangibleTimer=-1;rocket_caps_update();CHECK(!collected);
    cap.oIntangibleTimer=0;gMarioStates[1].pos[0]=500;rocket_caps_update();CHECK(!collected);
    gMarioStates[1].pos[0]=NAN;rocket_caps_update();CHECK(!collected);
    gMarioStates[1].pos[0]=0;fixtureNow+=2;rocket_caps_update();CHECK(!collected); /* stale pose */
    fresh();grant=pickup();gMarioStates[1].health=0xff;rocket_caps_update();CHECK(!rocket_wing_active(1));
    fresh();grant=pickup();gMarioStates[1].flags&=~MARIO_WING_CAP;rocket_caps_update();CHECK(!rocket_wing_active(1));
    fresh();grant=pickup();gNetworkPlayers[1].currLevelAreaSeqId++;rocket_caps_update();CHECK(!rocket_wing_active(1));
    fresh();grant=pickup();gNetworkPlayers[1].connected=false;rocket_caps_update();CHECK(!rocket_wing_active(1));
    fresh();gMarioStates[0].pos[0]=0;gMarioStates[1].pos[0]=1000;
    CHECK(rocket_caps_interact(&gMarioStates[0],&cap));CHECK(rocket_wing_active(0)&&collected==1);
    gMarioStates[0].action=ACT_READING_SIGN;rocket_caps_update();CHECK(gMarioStates[0].capTimer==90);
    gMarioStates[0].action=ACT_IDLE;rocket_caps_update();CHECK(gMarioStates[0].capTimer==89);
    CHECK(rocket_wing_boost_mode()==1);ordinaryMode=1;CHECK(rocket_wing_boost_mode()==1);
    gMarioStates[0].flags&=~MARIO_CAP_ON_HEAD;rocket_caps_update();CHECK(!rocket_wing_active(0));
    CHECK(rocket_wing_boost_mode()==1);ordinaryMode=0;CHECK(rocket_wing_boost_mode()==0);
    fresh();gMarioStates[0].pos[0]=0;CHECK(rocket_caps_interact(&gMarioStates[0],&cap));
    gMarioStates[0].capTimer=1;rocket_caps_update();CHECK(!rocket_wing_active(0)&&gMarioStates[0].capTimer==0);
    /* Owning-client removal may revoke only its current grant. */
    fresh();grant=pickup();struct Packet cancel={0};u8 cancellation[15]={0};
    memcpy(cancellation,grant.buffer+5,8);memcpy(cancellation+8,grant.buffer+17,4);memcpy(cancellation+12,grant.buffer+21,2);
    packet_init(&cancel,PACKET_ROCKET_CAP_CANCEL,true,PLMT_NONE);packet_write(&cancel,cancellation,sizeof cancellation);
    packet_set_destination(&cancel,0);cancel.localIndex=1;cancel.cursor=5;
    CHECK(rocket_caps_cancel_allowed(&cancel));bad=cancel;bad.buffer[13]^=1;CHECK(!rocket_caps_cancel_allowed(&bad));
    bad=cancel;bad.localIndex=2;CHECK(!rocket_caps_cancel_allowed(&bad));
    bad=cancel;bad.requestBroadcast=true;CHECK(!rocket_caps_cancel_allowed(&bad));
    rocket_caps_receive_cancel(&cancel);CHECK(!rocket_wing_active(1));CHECK(!rocket_caps_cancel_allowed(&cancel));
    fresh();grant=pickup();CHECK(!rocket_caps_cancel_allowed(&cancel)); /* prior grant cannot cancel a replacement */
    /* Native collect packets cannot steal a host-owned cap through relay. */
    fresh();struct Packet legacy={0};u32 wingId=1;float pos[3]={0};
    packet_init(&legacy,PACKET_COLLECT_ITEM,true,PLMT_AREA);packet_write(&legacy,&wingId,4);packet_write(&legacy,pos,12);
    legacy.localIndex=1;legacy.cursor=3;legacy.buffer[3]|=2;before=forwarded;packet_receive(&legacy);
    CHECK(forwarded==before&&!collected);
    /* Red-box object events are validated requests, not client raw fields. */
    struct Packet event={0};u8 owner=1;u32 syncId=55,boxId=2;u16 eventId=1,seed=0;
    packet_init(&event,PACKET_OBJECT,true,PLMT_AREA);packet_write(&event,&owner,1);packet_write(&event,&syncId,4);
    packet_write(&event,&eventId,2);packet_write(&event,&seed,2);packet_write(&event,&boxId,4);
    event.localIndex=1;event.cursor=10;event.destGlobalId=PACKET_DESTINATION_BROADCAST;
    unsigned sent=boxSent;CHECK(!rocket_caps_object_allowed(&event));CHECK(boxSent==sent+1&&box.oExclamationBoxForce);
    CHECK(!rocket_caps_object_allowed(&event));CHECK(boxSent==sent+1);
    box.oExclamationBoxForce=0;gMarioStates[1].pos[0]=1000;
    CHECK(!rocket_caps_object_allowed(&event));CHECK(boxSent==sent+1);
    gMarioStates[1].pos[0]=0;attack=0;CHECK(!rocket_caps_object_allowed(&event));CHECK(boxSent==sent+1);attack=INT_HIT_FROM_BELOW;
    /* Actual spawn preflight rejects cap/box creation, truncation and oversized counts. */
    struct Packet spawn={0};const unsigned stride=1+4+4+4+2+4*OBJECT_NUM_FIELDS+12+1+1+2;
    u8 spawnWire[1+stride];memset(spawnWire,0,sizeof spawnWire);spawnWire[0]=1;memcpy(spawnWire+10,&wingId,4);
    packet_init(&spawn,PACKET_SPAWN_OBJECTS,true,PLMT_AREA);packet_write(&spawn,spawnWire,sizeof spawnWire);spawn.cursor=10;
    CHECK(!network_caps_spawn_allowed(&spawn));memcpy(spawn.buffer+20,&boxId,4);CHECK(!network_caps_spawn_allowed(&spawn));
    u32 ordinaryId=3;memcpy(spawn.buffer+20,&ordinaryId,4);CHECK(network_caps_spawn_allowed(&spawn));
    spawn.dataLength--;CHECK(!network_caps_spawn_allowed(&spawn));spawn.dataLength++;spawn.buffer[10]=8;CHECK(!network_caps_spawn_allowed(&spawn));
    spawn.dataLength=PACKET_LENGTH;CHECK(!network_caps_spawn_allowed(&spawn));
    bad=legacy;bad.cursor=PACKET_LENGTH-16;bad.dataLength=PACKET_LENGTH;CHECK(!rocket_caps_item_allowed(&bad));
    bad=event;bad.cursor=PACKET_LENGTH-13;bad.dataLength=PACKET_LENGTH;CHECK(!rocket_caps_object_allowed(&bad));
    /* Native geometry and full-basis topper transform, plus character suspension. */
    fresh();gCLIOpts.offline=true;gNetworkType=NT_NONE;
    gMarioStates[0].flags|=MARIO_WING_CAP;gMarioStates[0].capTimer=90;
    RocketSnapshot pose={0};pose.basis[2]=pose.basis[3]=pose.basis[7]=1;pose.position[0]=100;pose.position[1]=200;pose.position[2]=300;
    sourceSnapshot=&pose;rocket_wing_topper_update();CHECK(visual[0].activeFlags&ACTIVE_FLAG_ACTIVE);
    CHECK(visual[0].oInteractType==0&&visual[0].oIntangibleTimer==-1);
    CHECK(visual[0].transform[0][0]==1&&visual[0].transform[1][1]==1&&visual[0].transform[2][2]==1);
    CHECK(visual[0].oPosX==100&&visual[0].oPosY==266&&visual[0].oPosZ==300);
    pose.basis[3]=-1;pose.basis[7]=-1;rocket_wing_topper_update();
    CHECK(visual[0].transform[0][0]==-1&&visual[0].transform[1][1]==-1&&visual[0].oPosY<200);
    /* Independent rigid-point checks: arbitrary yaw/pitch/roll and all points
     * of the native cap bottom stay in a single roof-local plane. */
    for(int angle=0;angle<72;angle++){
        Mat4 rotation;Vec3f origin={0,0,0};Vec3s euler={angle*911,angle*2053,angle*1297};
        mtxf_rotate_zxy_and_translate(rotation,origin,euler);
        for(int k=0;k<3;k++){
            pose.basis[k]=rotation[2][k];pose.basis[3+k]=rotation[0][k];pose.basis[6+k]=rotation[1][k];
        }
        ++gGlobalTimer;rocket_wing_topper_update();
        CHECK(visual[0].header.gfx.skipInterpolationTimestamp==gGlobalTimer);
        const float capPoints[][3]={{0,0,0},{-101,0,-118},{103,0,109},{0,144,0}};
        for(unsigned p=0;p<4;p++){
            float world[3];
            for(int k=0;k<3;k++){
                world[k]=visual[0].transform[3][k];
                for(int a=0;a<3;a++)world[k]+=capPoints[p][a]*.25f*visual[0].header.gfx.scale[a]*visual[0].transform[a][k];
            }
            const float local[]={capPoints[p][2]*.3375f,capPoints[p][0]*.3375f,66+capPoints[p][1]*.3375f};
            for(int a=0;a<3;a++){
                float dot=0;for(int k=0;k<3;k++)dot+=(world[k]-pose.position[k])*pose.basis[a*3+k];
                CHECK(fabsf(dot-local[a])<.002f);
            }
        }
        CHECK(gMarioStates[0].capTimer==90); /* rendering never consumes a cap */
    }
    memset(pose.basis,0,sizeof pose.basis);pose.basis[2]=pose.basis[3]=pose.basis[7]=1;
    pose.quicksand_depth=30;rocket_wing_topper_update();CHECK(visual[0].oPosY==236);
    gMarioStates[0].action=ACT_SQUISHED;gMarioStates[0].squishTimer=20;
    objects[0].header.gfx.scale[0]=objects[0].header.gfx.scale[2]=1.4f;objects[0].header.gfx.scale[1]=.4f;
    rocket_wing_topper_update();CHECK(fabsf(visual[0].oPosY-172.4f)<.001f);
    CHECK(visual[0].transform[0][0]==1.4f&&visual[0].transform[1][1]==.4f);
    gMarioStates[0].action=ACT_IDLE;gMarioStates[0].squishTimer=0;pose.quicksand_depth=0;
    /* Same priority as the actual world renderer, even if a stale runtime pose
     * is available during a native cutscene presentation handoff. */
    RocketSnapshot nativePose=pose;nativePose.position[0]=600;presentationSnapshot=&nativePose;
    rocket_wing_topper_update();CHECK(visual[0].oPosX==600);presentationSnapshot=NULL;
    sourceSnapshot=NULL;rocket_wing_topper_update();CHECK(visual[0].activeFlags==ACTIVE_FLAG_DEACTIVATED);
    CHECK(gMarioStates[0].capTimer==90); /* switching does not restart a native timer */
    gMarioStates[0].capTimer=0;CHECK(!rocket_wing_active(0)&&rocket_wing_boost_mode()==0);
    gMarioStates[0].capTimer=90;gMarioStates[0].health=0xff;CHECK(!rocket_wing_active(0));
    /* Remote car uses the same native topper and accepted rigid pose. */
    fresh();grant=pickup();CharacterNetState remote={0};remote.speed_percent=100;remote.kind=CNET_OCTANE;remote.active=1;remote.interaction=1;remote.sequence=2;remote.area_sequence=5;
    remote.car.basis[2]=remote.car.basis[3]=remote.car.basis[7]=1;
    for(int k=0;k<4;k++)remote.car.wheel_radius[k]=32;
    CHECK(character_net_accept(1,&remote));rocket_wing_topper_update();CHECK(visual[1].activeFlags&ACTIVE_FLAG_ACTIVE);
    CHECK(visual[1].header.gfx.throwMatrix==&visual[1].transform&&visual[1].oPosY==66);
    remote.car.basis[3]=remote.car.basis[7]=-1;remote.epoch++;remote.sequence++;
    CHECK(character_net_accept(1,&remote));rocket_wing_topper_update();
    CHECK(visual[1].oPosY==-66&&visual[1].transform[0][0]==-1&&visual[1].transform[1][1]==-1);
    remote.active=0;remote.interaction=0;remote.sequence++;CHECK(character_net_accept(1,&remote));rocket_wing_topper_update();
    CHECK(visual[1].activeFlags==ACTIVE_FLAG_DEACTIVATED&&gMarioStates[1].capTimer==90);
    remote.car.basis[3]=remote.car.basis[7]=1; /* later pickup scenarios are upright */
    /* Host native pause is replicated; a lost host cannot freeze a client indefinitely. */
    fresh();grant=pickup();as_client();grant.buffer[26]=1;receive(grant);
    rocket_caps_update();CHECK(gMarioStates[0].capTimer==90);fixtureNow+=1.1;rocket_caps_update();CHECK(gMarioStates[0].capTimer==89);
    /* All three native caps share one additive mask and max(duration, timer). */
    fresh();gLevelValues.metalCapDuration=45;gLevelValues.vanishCapDuration=70;cap.behavior=bhvMetalCap;
    capturedPacket=&grant;rocket_caps_update();capturedPacket=NULL;
    CHECK(rocket_caps_active_flags(1)==MARIO_METAL_CAP&&rocket_caps_remaining(1)==45);
    CHECK(!rocket_wing_active(1));CHECK(collected==1);
    cap.behavior=bhvVanishCap;cap.oInteractStatus=0;capturedPacket=&grant;rocket_caps_update();capturedPacket=NULL;
    CHECK(rocket_caps_active_flags(1)==(MARIO_METAL_CAP|MARIO_VANISH_CAP)&&rocket_caps_remaining(1)==70);
    gLevelValues.wingCapDuration=20;cap.behavior=bhvWingCap;cap.oInteractStatus=0;
    capturedPacket=&grant;rocket_caps_update();capturedPacket=NULL;
    CHECK(rocket_caps_active_flags(1)==MARIO_SPECIAL_CAPS&&rocket_caps_remaining(1)==69&&collected==3);
    as_client();receive(grant);CHECK(rocket_caps_active_flags(0)==MARIO_SPECIAL_CAPS&&rocket_caps_remaining(0)==69);
    CHECK(rocket_wing_boost_mode()==1);
    bad=grant;bad.buffer[13]++;bad.buffer[27]|=0x10;receive(bad);CHECK(rocket_caps_remaining(0)==69);
    for(unsigned timer=68;timer>0;--timer){
        rocket_caps_update();CHECK(rocket_caps_remaining(0)==timer);
        u32 expected=timer<64&&((UINT64_C(0x4444449249255555)>>timer)&1)?0:MARIO_SPECIAL_CAPS;
        CHECK((rocket_caps_visual_flags(0)&MARIO_SPECIAL_CAPS)==expected);
    }
    rocket_caps_update();CHECK(!rocket_caps_active_flags(0)&&!rocket_caps_remaining(0)&&rocket_wing_boost_mode()==0);
    /* Native removal of just Wing retains legitimate Metal/Vanish, and cannot
     * be undone by a heartbeat or upgraded by a cancellation packet. */
    fresh();cap.behavior=bhvMetalCap;rocket_caps_update();cap.behavior=bhvWingCap;cap.oInteractStatus=0;
    capturedPacket=&grant;rocket_caps_update();capturedPacket=NULL;
    memset(cancellation,0,sizeof cancellation);memcpy(cancellation,grant.buffer+5,8);
    memcpy(cancellation+8,grant.buffer+17,4);memcpy(cancellation+12,grant.buffer+21,2);cancellation[14]=MARIO_METAL_CAP;
    packet_init(&cancel,PACKET_ROCKET_CAP_CANCEL,true,PLMT_NONE);packet_write(&cancel,cancellation,sizeof cancellation);
    packet_set_destination(&cancel,0);cancel.localIndex=1;cancel.cursor=5;CHECK(rocket_caps_cancel_allowed(&cancel));
    bad=cancel;bad.buffer[19]|=MARIO_VANISH_CAP;CHECK(!rocket_caps_cancel_allowed(&bad));
    rocket_caps_receive_cancel(&cancel);CHECK(rocket_caps_active_flags(1)==MARIO_METAL_CAP&&rocket_caps_remaining(1)>0);
    as_client();receive(grant);gMarioStates[0].flags&=~MARIO_WING_CAP;rocket_caps_update();
    CHECK(rocket_caps_active_flags(0)==MARIO_METAL_CAP&&rocket_wing_boost_mode()==0);
    bad=grant;bad.buffer[13]+=2;receive(bad);CHECK(rocket_caps_active_flags(0)==MARIO_METAL_CAP);
    gMarioStates[0].flags|=MARIO_WING_CAP|MARIO_VANISH_CAP;rocket_caps_before_mario_update(&gMarioStates[0]);
    CHECK((gMarioStates[0].flags&MARIO_SPECIAL_CAPS)==MARIO_METAL_CAP);
    /* Gameplay contact uses the newest accepted driving pose, not render interpolation. */
    fresh();remote.car.position[0]=1000;remote.sequence=2;remote.active=1;remote.interaction=1;
    CHECK(character_net_accept(1,&remote));rocket_caps_update();CHECK(!collected);
    remote.car.position[0]=0;remote.sequence++;CHECK(character_net_accept(1,&remote));rocket_caps_update();CHECK(collected==1);
    /* Existing native winged-metal geometry and vanish transparency compose. */
    fresh();gCLIOpts.offline=true;gNetworkType=NT_NONE;gMarioStates[0].flags|=MARIO_SPECIAL_CAPS;gMarioStates[0].capTimer=90;
    sourceSnapshot=&pose;rocket_wing_topper_update();CHECK(topperModel==MODEL_MARIOS_WINGED_METAL_CAP&&visual[0].oOpacity==128);
    sourceSnapshot=NULL;
    /* Every cap type is protected against legacy collect/spawn bypasses. */
    fresh();
    for(u32 capId=4;capId<=5;capId++){
        bad=legacy;bad.cursor=10;memcpy(bad.buffer+10,&capId,4);CHECK(!rocket_caps_item_allowed(&bad));
        spawn.buffer[10]=1;spawn.dataLength=10+sizeof spawnWire;memcpy(spawn.buffer+20,&capId,4);
        CHECK(!network_caps_spawn_allowed(&spawn));
    }
    /* Native course entry is a host-derived grant once per synchronized epoch.
     * Flags, local removal, expiry and duplicate updates never reissue it. */
    const int courses[]={COURSE_COTMC,COURSE_TOTWC,COURSE_VCUTM};
    const int levels[]={LEVEL_COTMC,LEVEL_TOTWC,LEVEL_VCUTM};
    const u32 powers[]={MARIO_METAL_CAP,MARIO_WING_CAP,MARIO_VANISH_CAP};
    for(unsigned c=0;c<3;c++){
        fresh();cap.activeFlags=0;
        gCurrCourseNum=courses[c];gCurrLevelNum=levels[c];
        gLevelValues.metalCapDurationCotmc=4;gLevelValues.wingCapDurationTotwc=4;gLevelValues.vanishCapDurationVcutm=4;
        for(unsigned i=0;i<2;i++){
            gNetworkPlayers[i].currCourseNum=courses[c];gNetworkPlayers[i].currLevelNum=levels[c];
        }
        rocket_caps_before_mario_update(&gMarioStates[0]);
        CHECK(rocket_caps_active_flags(0)==powers[c]&&rocket_caps_remaining(0)==4);
        rocket_caps_update();CHECK(rocket_caps_active_flags(1)==powers[c]&&rocket_caps_remaining(1)==3);
        for(unsigned j=0;j<4;j++){rocket_caps_before_mario_update(&gMarioStates[0]);rocket_caps_update();}
        CHECK(!rocket_caps_active_flags(0)&&!rocket_caps_active_flags(1)&&!collected);
        gMarioStates[1].flags|=powers[c];gMarioStates[1].capTimer=65535;rocket_caps_apply(&gMarioStates[1]);
        rocket_caps_update();CHECK(!rocket_caps_active_flags(1));
    }
    fresh();cap.activeFlags=0;gCurrCourseNum=COURSE_TOTWC;gCurrLevelNum=LEVEL_BOB;
    rocket_caps_update();CHECK(!rocket_caps_active_flags(0)&&!rocket_caps_active_flags(1));
    fresh();cap.activeFlags=0;gCurrCourseNum=COURSE_TOTWC;gCurrLevelNum=LEVEL_TOTWC;
    as_client();gMarioStates[0].flags|=MARIO_WING_CAP;gMarioStates[0].capTimer=65535;
    rocket_caps_before_mario_update(&gMarioStates[0]);rocket_caps_update();CHECK(!rocket_caps_active_flags(0));
    chassis_authority_tests();
    printf("Shared cap authority/ingress/topper: %d checks passed\n",checks);return 0;
}
