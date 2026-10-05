/* Real packet ingress, rule service and packet writer; inert host services. */
#define ROCKET_BOOST_REAL_TEST
#include "character_transport_fixture.h"
#include "../../../src/pc/rocket_boost.h"
#include "../../../src/pc/network/version.h"
unsigned configRocketBoostMode,configRocketSurfaceMode;
static int saves;
void configfile_save(const char *name){CHECK(!strcmp(name,"fixture.cfg"));saves++;}
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
    CHECK(!strcmp(get_version()+strlen(get_version())-strlen("-wheel1"),"-wheel1"));
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
    bad=native;bad.buffer[6]=200;bad.buffer[bad.dataLength-1]=2;receive(bad);CHECK(rocket_surface_mode()==0);
    bad=initial;bad.buffer[bad.dataLength-1]=255;
    client();gNetworkPlayerLocal=gNetworkPlayerServer=NULL;CHECK(!rocket_boost_join_valid(&bad));
    host();int relayBefore=forwarded;bad=native;bad.localIndex=1;bad.cursor=3;bad.buffer[3]|=2;
    packet_receive(&bad);CHECK(forwarded==relayBefore);
    rocket_boost_session_reset();configRocketSurfaceMode=1;next=join();accept_join(next);
    CHECK(rocket_surface_mode()==1);bad=compat;bad.buffer[6]=200;receive(bad);CHECK(rocket_surface_mode()==1);
    configRocketSurfaceMode=0;rocket_boost_session_reset();CHECK(rocket_surface_mode()==0);
    gNetworkType=NT_NONE;CHECK(rocket_surface_set_mode(1));CHECK(rocket_surface_mode()==1);
    configRocketSurfaceMode=255;CHECK(rocket_surface_mode()==0);
    CHECK(strstr(get_version(),"env1")!=NULL);
    printf("boost and surface rules: %d checks passed\n",checks);return 0;
}
