/* Actual native packet fixture with opt-in QA observation enabled. No sockets. */
#define main original_coin_checks
#include "test_coin_boost.c"
#undef main
#include <stdlib.h>
u32 gGlobalTimer=1;
int rocket_runtime_snapshot(RocketSnapshot *state){memset(state,0,sizeof(*state));state->boost=fuel;return owns;}
int rocket_runtime_boost_mode(void){return ROCKET_BOOST_COIN_ONLY;}
int rocket_boost_mode(void){return ROCKET_BOOST_COIN_ONLY;}
uint32_t rocket_caps_active_flags(unsigned index){(void)index;return 0;}
int main(void){
    gCLIOpts.loopbackOnly=true;
    assert(setenv("SM64_ROCKET_QA_COIN_FOLLOWUP","1",1)==0);
    assert(original_coin_checks()==0);
    RocketCoinQaCredit before,after;
    rocket_coin_qa_credit_snapshot(&before);
    assert(before.serial>0);
    select_peer(0);gCLIOpts.characterNet=true;
    pickup();rocket_coin_qa_credit_snapshot(&after);
    assert(after.serial==before.serial+1&&after.collector==0&&after.before==40&&after.after==45&&after.active&&after.result);
    before=after;
    pickup();rocket_coin_qa_credit_snapshot(&after);assert(after.serial==before.serial);
    assert(unsetenv("SM64_ROCKET_QA_COIN_FOLLOWUP")==0);
    select_peer(0);pickup();rocket_coin_qa_credit_snapshot(&after);
    assert(fuel==45&&creditCount==1&&after.serial==before.serial);
    puts("coin QA telemetry: PASS actual accounting unchanged, one application counted, duplicate suppressed, opt-out inert");
    return 0;
}
