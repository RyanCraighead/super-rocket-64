/* Verbatim native join-request function, real packet reader and version. */
#include "pc/network/network.h"
#include "pc/network/version.h"
#include "pc/configfile.h"
#include "pc/cliopts.h"
#include "pc/debuglog.h"
#include <stdlib.h>
#include <string.h>
static int checks,joins;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#undef SOFT_ASSERT
#define SOFT_ASSERT(x) CHECK(x)
struct CLIOptions gCLIOpts;
enum NetworkType gNetworkType;
const struct PlayerPalette DEFAULT_MARIO_PALETTE={0};
static u8 sJoinRequestPlayerModel;
static struct PlayerPalette sJoinRequestPlayerPalette;
static char sJoinRequestPlayerName[MAX_CONFIG_STRING];
void network_send_join(struct Packet *p){CHECK(!p->error);joins++;}
#include "surface_join_request.inc.c"
int main(void){
    gNetworkType=NT_SERVER;gCLIOpts.characterNet=true;
    struct Packet valid={0};valid.cursor=5;
    valid.dataLength=5+MAX_VERSION_LENGTH+1+sizeof(struct PlayerPalette)+MAX_CONFIG_STRING;
    memcpy(valid.buffer+5,get_version(),strlen(get_version()));
    struct Packet p=valid;network_receive_join_request(&p);CHECK(joins==1&&p.cursor==p.dataLength);
    p=valid;char *env=strstr((char*)p.buffer+5,"-env3-");CHECK(env);env[4]='2';
    network_receive_join_request(&p);CHECK(joins==1); // older wall policy rejected
    p=valid;p.dataLength--;network_receive_join_request(&p);CHECK(joins==1);
    p=valid;p.dataLength++;network_receive_join_request(&p);CHECK(joins==1);
    p=valid;memset(p.buffer+5,'x',MAX_VERSION_LENGTH);network_receive_join_request(&p);CHECK(joins==1);
    p=valid;p.dataLength=5;network_receive_join_request(&p);CHECK(joins==1);
    gCLIOpts.characterNet=false;network_receive_join_request(&p);CHECK(joins==2);
    printf("native join version gate: %d checks passed\n",checks);return 0;
}
