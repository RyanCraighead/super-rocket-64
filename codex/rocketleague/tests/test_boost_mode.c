/* Real packet ingress, rule service and packet writer; inert host services. */
#define ROCKET_BOOST_REAL_TEST
#include "character_transport_fixture.h"
#include "../../../src/pc/rocket_boost.h"
#include "../../../src/pc/network/version.h"
unsigned configRocketBoostMode,configRocketSurfaceMode,configRocketSpeedPercent=75,configRocketJumpPercent=50;
static int saves,saveFails;
static unsigned savedSpeed,savedJump;
void configfile_save(const char *name){CHECK(!strcmp(name,"fixture.cfg"));saves++;}
int configfile_save_atomic(const char *name){if(saveFails)return 0;configfile_save(name);savedSpeed=configRocketSpeedPercent;savedJump=configRocketJumpPercent;return 1;}
const char *configfile_name(void){return "fixture.cfg";}
static bool match(void *a,void *b){return a==b;}
static void host(void){
    gNetworkType=NT_SERVER;gNetworkPlayerServer=gNetworkPlayerLocal=&gNetworkPlayers[0];
    gNetworkServerAddr=NULL;
}
static void client(void){
    gNetworkType=NT_CLIENT;gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkPlayerServer=&gNetworkPlayers[1];
    gNetworkServerAddr=(void*)1;
}
static void receive(struct Packet p){
    p.localIndex=1;p.addr=(void*)1;p.cursor=3;
    packet_receive(&p);
}
static struct Packet join(void){
    struct Packet p={0};packet_init(&p,PACKET_JOIN,true,PLMT_NONE);
    u8 prefix[MAX_VERSION_LENGTH+1+sizeof(s16)+11+512]={0};
    memcpy(prefix,get_version(),strlen(get_version()));
    packet_write(&p,prefix,sizeof prefix);rocket_boost_write_rule(&p);
    p.localIndex=UNKNOWN_LOCAL_INDEX;p.addr=(void*)1;p.cursor=5;return p;
}
static void accept_join(struct Packet p){
    client();gNetworkPlayerLocal=gNetworkPlayerServer=NULL;rocket_boost_session_reset();
    CHECK(rocket_boost_join_valid(&p));p.cursor=p.dataLength-ROCKET_SESSION_RULE_BYTES;rocket_boost_read_join(&p);client();
}
#include "test_difficulty_ui.inc.c"
int main(void){
    static struct NetworkSystem system={.get_id_str=id,.match_addr=match,.requireServerBroadcast=true};
    gNetworkSystem=&system;gCLIOpts.characterNet=true;
    for(int i=0;i<3;i++){gNetworkPlayers[i].connected=true;gNetworkPlayers[i].localIndex=i;gNetworkPlayers[i].globalIndex=i;}
    gNetworkType=NT_NONE;
    CHECK(rocket_boost_mode()==ROCKET_BOOST_COIN_ONLY);
    CHECK(rocket_boost_can_set_mode());CHECK(!rocket_boost_set_mode(2));CHECK(!saves);
    CHECK(rocket_boost_set_mode(1));CHECK(saves==1);CHECK(rocket_boost_mode()==1);
    CHECK(rocket_boost_set_mode(1));CHECK(saves==1);
    configRocketBoostMode=255;CHECK(rocket_boost_mode()==0);configRocketBoostMode=1;
    host();rocket_boost_session_reset();struct Packet initial=join(), infinite={0}, finite={0};
    capturedPacket=&infinite;rocket_boost_network_update();capturedPacket=NULL;
    CHECK(infinite.reliable&&infinite.packetType==PACKET_ROCKET_BOOST_RULE);
    capturedPacket=&finite;CHECK(rocket_boost_set_mode(0));capturedPacket=NULL;
    CHECK(finite.dataLength==infinite.dataLength);CHECK(saves==2);
    struct Packet heartbeat={0};capturedPacket=&heartbeat;rocket_boost_network_update();
    CHECK(!heartbeat.dataLength);fixtureNow+=1.1;rocket_boost_network_update();capturedPacket=NULL;
    CHECK(heartbeat.dataLength==finite.dataLength);
    configRocketBoostMode=1; /* Client's personal preference must survive the host rule. */
    accept_join(initial);CHECK(rocket_boost_mode()==1);
    CHECK(!rocket_boost_can_set_mode());CHECK(!rocket_boost_set_mode(0));CHECK(saves==2&&configRocketBoostMode==1);
    receive(heartbeat);CHECK(rocket_boost_mode()==0);CHECK(configRocketBoostMode==1);
    receive(infinite);CHECK(rocket_boost_mode()==0); /* old revision */
    receive(finite);CHECK(rocket_boost_mode()==0); /* duplicate */
    struct Packet bad=infinite;bad.buffer[6]=200;bad.localIndex=2;bad.cursor=3;bad.addr=(void*)2;
    packet_receive(&bad);CHECK(rocket_boost_mode()==0); /* another client */
    bad=infinite;bad.buffer[6]=200;bad.buffer[5]=2;receive(bad);CHECK(rocket_boost_mode()==0);
    bad=infinite;bad.buffer[6]=200;bad.dataLength--;receive(bad);CHECK(rocket_boost_mode()==0);
    bad=infinite;bad.buffer[6]=200;bad.dataLength++;receive(bad);CHECK(rocket_boost_mode()==0);
    bad=infinite;bad.buffer[6]=200;bad.buffer[3]|=2;receive(bad);CHECK(rocket_boost_mode()==0);
    bad=infinite;bad.buffer[6]=200;bad.buffer[4]=2;receive(bad);CHECK(rocket_boost_mode()==0);
    bad=infinite;bad.buffer[6]=200;bad.buffer[10]^=1;receive(bad);CHECK(rocket_boost_mode()==0);
    bad=infinite;bad.buffer[6]=200;bad.localIndex=1;bad.cursor=3;bad.addr=(void*)2;
    packet_receive(&bad);CHECK(rocket_boost_mode()==0); /* wrong host address */
    /* Forged client rule must never take the directed or broadcast relay path. */
    host();int before=forwarded;
    bad=infinite;bad.localIndex=1;bad.cursor=3;bad.buffer[3]|=2;packet_receive(&bad);CHECK(forwarded==before);
    bad=infinite;bad.localIndex=1;bad.cursor=3;bad.buffer[4]=2;packet_receive(&bad);CHECK(forwarded==before);
    /* Malformed joins are rejected before any session rule can apply. */
    client();gNetworkPlayerLocal=gNetworkPlayerServer=NULL;rocket_boost_session_reset();
    CHECK(rocket_boost_mode()==0);CHECK(rocket_boost_join_valid(&initial));
    bad=initial;bad.dataLength--;CHECK(!rocket_boost_join_valid(&bad));
    bad=initial;bad.dataLength++;CHECK(!rocket_boost_join_valid(&bad));
    bad=initial;bad.buffer[bad.dataLength-ROCKET_SESSION_RULE_BYTES]=255;CHECK(!rocket_boost_join_valid(&bad));
    bad=initial;memset(bad.buffer+bad.dataLength-ROCKET_SESSION_RULE_BYTES+1,0,4);CHECK(!rocket_boost_join_valid(&bad));
    bad=initial;memset(bad.buffer+bad.dataLength-ROCKET_SESSION_RULE_BYTES+5,0,8);CHECK(!rocket_boost_join_valid(&bad));
    bad=initial;bad.addr=(void*)2;CHECK(!rocket_boost_join_valid(&bad));
    bad=initial;bad.localIndex=2;CHECK(!rocket_boost_join_valid(&bad));
    bad=initial;bad.error=true;CHECK(!rocket_boost_join_valid(&bad));
    receive(infinite);CHECK(rocket_boost_mode()==0); /* no live rule before join */
    /* Reconnect to a new session, then replay the old session's newer revision. */
    host();rocket_boost_session_reset();configRocketBoostMode=0;struct Packet next=join();
    accept_join(next);bad=infinite;bad.buffer[6]=200;receive(bad);CHECK(rocket_boost_mode()==0);
    CHECK(strstr(rocket_boost_scope_label(),"host"));
    rocket_boost_session_reset();CHECK(strstr(rocket_boost_scope_label(),"Waiting"));
    gNetworkType=NT_NONE;configRocketBoostMode=1;CHECK(rocket_boost_mode()==1);
    CHECK(!strcmp(rocket_boost_mode_label(),"Infinite"));
    gNetworkType=NT_CLIENT;gCLIOpts.characterNet=false;CHECK(rocket_boost_mode()==0);
    CHECK(strstr(get_version(),"boost-mode1")==NULL);gCLIOpts.characterNet=true;
    CHECK(strstr(get_version(),"boost-mode1")!=NULL);
    CHECK(!strcmp(get_version()+strlen(get_version())-strlen("-wheel1-bump1-tune2"),"-wheel1-bump1-tune2"));
    CHECK(strlen(SM64COOPDX_VERSION)+strlen(CNET_VERSION_SUFFIX)<MAX_VERSION_LENGTH);
    /* Both rules share one session/revision; client preferences never win. */
    host();rocket_boost_session_reset();configRocketBoostMode=1;configRocketSurfaceMode=1;
    initial=join();struct Packet native={0},compat={0};
    capturedPacket=&native;rocket_boost_network_update();capturedPacket=NULL;
    int oldSaves=saves;capturedPacket=&compat;CHECK(rocket_surface_set_mode(0));capturedPacket=NULL;
    CHECK(saves==oldSaves+1);CHECK(!rocket_surface_set_mode(2));
    configRocketSurfaceMode=0;accept_join(initial);CHECK(rocket_surface_mode()==1&&rocket_boost_mode()==1);
    CHECK(!rocket_surface_set_mode(0)&&configRocketSurfaceMode==0);
    receive(compat);CHECK(rocket_surface_mode()==0&&rocket_boost_mode()==1);
    receive(native);CHECK(rocket_surface_mode()==0);
    bad=native;bad.buffer[6]=200;bad.buffer[bad.dataLength-3]=2;receive(bad);CHECK(rocket_surface_mode()==0);
    bad=initial;bad.buffer[bad.dataLength-3]=255;
    client();gNetworkPlayerLocal=gNetworkPlayerServer=NULL;CHECK(!rocket_boost_join_valid(&bad));
    host();int relayBefore=forwarded;bad=native;bad.localIndex=1;bad.cursor=3;bad.buffer[3]|=2;
    packet_receive(&bad);CHECK(forwarded==relayBefore);
    rocket_boost_session_reset();configRocketSurfaceMode=1;next=join();accept_join(next);
    CHECK(rocket_surface_mode()==1);bad=compat;bad.buffer[6]=200;receive(bad);CHECK(rocket_surface_mode()==1);
    configRocketSurfaceMode=0;rocket_boost_session_reset();CHECK(rocket_surface_mode()==0);
    gNetworkType=NT_NONE;CHECK(rocket_surface_set_mode(1));CHECK(rocket_surface_mode()==1);
    configRocketSurfaceMode=255;CHECK(rocket_surface_mode()==1);
    CHECK(strstr(get_version(),"env1")!=NULL);
    /* Speed shares the same authenticated join/session/revision contract. */
    gNetworkType=NT_NONE;configRocketSpeedPercent=0;CHECK(rocket_speed_percent()==100);
    CHECK(!rocket_speed_set_percent(0)&&!rocket_speed_set_percent(49)&&!rocket_speed_set_percent(101));
    CHECK(rocket_speed_set_percent(75));oldSaves=saves;CHECK(rocket_speed_set_percent(75));CHECK(saves==oldSaves);
    host();rocket_boost_session_reset();initial=join();u32 firstRule=rocket_rule_revision();
    struct Packet fullSpeed={0},slowSpeed={0};
    capturedPacket=&fullSpeed;CHECK(rocket_speed_set_percent(100));capturedPacket=NULL;
    CHECK(rocket_rule_revision()>firstRule&&fullSpeed.dataLength);
    capturedPacket=&slowSpeed;CHECK(rocket_speed_set_percent(50));capturedPacket=NULL;
    struct Packet late=join();configRocketSpeedPercent=100;accept_join(initial);
    CHECK(rocket_speed_percent()==75&&rocket_speed_scale()==.75f&&configRocketSpeedPercent==100);
    oldSaves=saves;CHECK(!rocket_speed_set_percent(50)&&saves==oldSaves);
    receive(fullSpeed);CHECK(rocket_speed_percent()==100);u32 currentRule=rocket_rule_revision();
    receive(fullSpeed);CHECK(rocket_rule_revision()==currentRule);
    receive(slowSpeed);CHECK(rocket_speed_percent()==50);
    receive(fullSpeed);CHECK(rocket_speed_percent()==50); /* older rule */
    for(unsigned invalidSpeed=0;invalidSpeed<3;invalidSpeed++){
        bad=fullSpeed;bad.buffer[6]=200;bad.buffer[bad.dataLength-2]=invalidSpeed==0?0:invalidSpeed==1?49:101;
        receive(bad);CHECK(rocket_speed_percent()==50);
    }
    bad=fullSpeed;bad.buffer[6]=200;bad.localIndex=2;bad.cursor=3;bad.addr=(void*)2;
    packet_receive(&bad);CHECK(rocket_speed_percent()==50);
    accept_join(late);CHECK(rocket_speed_percent()==50&&configRocketSpeedPercent==100);
    host();rocket_boost_session_reset();configRocketSpeedPercent=75;next=join();
    configRocketSpeedPercent=100;accept_join(next);receive(slowSpeed);CHECK(rocket_speed_percent()==75);
    rocket_boost_session_reset();CHECK(rocket_speed_percent()==100); /* waiting for authenticated join */
    gNetworkType=NT_NONE;CHECK(rocket_speed_percent()==100); /* saved offline choice survived */
    /* Independent jump-height preference, authenticated host rule and late joins. */
    configRocketJumpPercent=0;CHECK(rocket_jump_percent()==100);
    CHECK(!rocket_jump_set_percent(0)&&!rocket_jump_set_percent(29)&&!rocket_jump_set_percent(101));
    CHECK(rocket_jump_set_percent(50));oldSaves=saves;CHECK(rocket_jump_set_percent(50));CHECK(saves==oldSaves);
    u32 offlineRule=rocket_rule_revision();CHECK(rocket_jump_set_percent(100));
    CHECK(rocket_rule_revision()!=offlineRule&&rocket_speed_percent()==100);
    CHECK(rocket_jump_set_percent(50)&&rocket_rule_revision()==offlineRule);
    host();rocket_boost_session_reset();configRocketSpeedPercent=75;initial=join();firstRule=rocket_rule_revision();
    struct Packet fullJump={0},halfJump={0};
    capturedPacket=&fullJump;CHECK(rocket_jump_set_percent(100));capturedPacket=NULL;
    CHECK(fullJump.dataLength&&rocket_rule_revision()>firstRule&&rocket_speed_percent()==75);
    u32 fullJumpRule=rocket_rule_revision();
    capturedPacket=&halfJump;CHECK(rocket_jump_set_percent(50));capturedPacket=NULL;
    CHECK(rocket_rule_revision()>fullJumpRule); /* A-B-A must retire old owner poses/grants. */
    late=join();configRocketJumpPercent=100;accept_join(initial);
    CHECK(rocket_jump_percent()==50&&configRocketJumpPercent==100&&rocket_speed_percent()==75);
    oldSaves=saves;CHECK(!rocket_jump_set_percent(50)&&saves==oldSaves);
    CharacterNetState oldPose={0};oldPose.kind=CNET_MARIO;oldPose.sequence=1;
    oldPose.speed_percent=rocket_speed_percent();oldPose.rule_revision=rocket_rule_revision();
    CHECK(character_net_accept(1,&oldPose));
    receive(fullJump);CHECK(rocket_jump_percent()==100);currentRule=rocket_rule_revision();
    oldPose.sequence++;CHECK(!character_net_accept(1,&oldPose));
    CharacterNetState fullPose=oldPose;fullPose.rule_revision=currentRule;CHECK(character_net_accept(1,&fullPose));
    receive(fullJump);CHECK(rocket_rule_revision()==currentRule);
    receive(halfJump);CHECK(rocket_jump_percent()==50&&rocket_rule_revision()>currentRule);
    fullPose.sequence++;CHECK(!character_net_accept(1,&fullPose));
    oldPose.sequence=fullPose.sequence;CHECK(!character_net_accept(1,&oldPose));
    oldPose.rule_revision=rocket_rule_revision();CHECK(character_net_accept(1,&oldPose));
    receive(fullJump);CHECK(rocket_jump_percent()==50);
    for(unsigned invalidJump=0;invalidJump<3;invalidJump++){
        bad=fullJump;bad.buffer[6]=200;bad.buffer[bad.dataLength-1]=invalidJump==0?0:invalidJump==1?29:101;
        receive(bad);CHECK(rocket_jump_percent()==50);
        bad=initial;bad.buffer[bad.dataLength-1]=invalidJump==0?0:invalidJump==1?29:101;
        client();gNetworkPlayerLocal=gNetworkPlayerServer=NULL;CHECK(!rocket_boost_join_valid(&bad));client();
    }
    bad=fullJump;bad.buffer[6]=200;bad.localIndex=2;bad.cursor=3;bad.addr=(void*)2;
    packet_receive(&bad);CHECK(rocket_jump_percent()==50);
    bad=fullJump;bad.buffer[6]=200;bad.localIndex=1;bad.cursor=3;bad.addr=(void*)2;
    packet_receive(&bad);CHECK(rocket_jump_percent()==50);
    bad=fullJump;bad.buffer[6]=200;bad.dataLength--;receive(bad);CHECK(rocket_jump_percent()==50);
    bad=fullJump;bad.buffer[6]=200;bad.dataLength++;receive(bad);CHECK(rocket_jump_percent()==50);
    accept_join(late);CHECK(rocket_jump_percent()==50&&configRocketJumpPercent==100);
    host();int oldForwarded=forwarded;bad=fullJump;bad.localIndex=1;bad.cursor=3;bad.buffer[3]|=2;
    packet_receive(&bad);CHECK(forwarded==oldForwarded);
    rocket_boost_session_reset();configRocketJumpPercent=75;next=join();
    configRocketJumpPercent=100;accept_join(next);receive(halfJump);CHECK(rocket_jump_percent()==75);
    CHECK(strstr(rocket_jump_scope_label(),"host"));
    rocket_boost_session_reset();CHECK(rocket_jump_percent()==100&&strstr(rocket_jump_scope_label(),"Waiting"));
    gNetworkType=NT_NONE;CHECK(rocket_jump_percent()==100&&configRocketJumpPercent==100);
    difficulty_tests();
    printf("boost and surface rules: %d checks passed\n",checks);return 0;
}
