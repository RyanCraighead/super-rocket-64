/* Included after CoinBoostEvent. Never changes an accounting decision. */
#ifdef ROCKET_CAR_QA
#include "pc/rocket_coin_qa.h"
#include "pc/rocket_boost.h"
#include "game/rocket_caps.h"
#include "game/display.h"
#include <stdlib.h>
static RocketCoinQaCredit coinQaCredit;
int rocket_coin_qa_enabled(void) {
    const char *opt=getenv("SM64_ROCKET_QA_COIN_FOLLOWUP");
    return gCLIOpts.characterNet&&gCLIOpts.loopbackOnly&&opt&&!strcmp(opt,"1");
}
void rocket_coin_qa_credit_snapshot(RocketCoinQaCredit *out) { if(out)*out=coinQaCredit; }
static void coin_qa_event(const char *stage,const CoinBoostEvent *e) {
    if(!rocket_coin_qa_enabled())return;
    RocketSnapshot pose={0};int active=rocket_runtime_snapshot(&pose);
    fprintf(stderr,"ROCKET_COIN_QA_EVENT {\"stage\":\"%s\",\"timer\":%u,\"global\":%u,\"collector\":%u,\"authority\":%u,\"request\":%u,\"epoch\":%u,\"area_sequence\":%u,\"authority_sequence\":%u,\"level\":%d,\"area\":%d,\"sync\":%u,\"behavior\":%u,\"parent\":%u,\"ordinal\":%u,\"origin\":[%.5f,%.5f,%.5f],\"value\":%d,\"coins\":%d,\"active\":%d,\"fuel\":%.5f,\"ordinary\":%d,\"effective\":%d,\"water\":%d,\"caps\":%u}\n",
        stage,gGlobalTimer,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:255,e->collector,e->authority,
        e->request,e->epoch,e->areaSequence,e->authoritySequence,gCurrLevelNum,e->area,e->syncId,e->behaviorId,
        e->parent,e->ordinal,e->home[0],e->home[1],e->home[2],e->value,gMarioStates[0].numCoins,
        active,pose.boost,rocket_boost_mode(),rocket_runtime_boost_mode(),pose.water_mode,rocket_caps_active_flags(0));
}
static void coin_qa_apply_credit(const CoinBoostEvent *e) {
    if(!rocket_coin_qa_enabled()){rocket_runtime_collect_coin();return;}
    RocketSnapshot before={0},after={0};int active=rocket_runtime_snapshot(&before);
    int result=rocket_runtime_collect_coin();
    int afterActive=rocket_runtime_snapshot(&after);
    coinQaCredit=(RocketCoinQaCredit){coinQaCredit.serial+1,e->request,e->epoch,e->parent,e->ordinal,
        e->syncId,e->collector,before.boost,after.boost,result,active&&afterActive};
    coin_qa_event("credit",e);
    fprintf(stderr,"ROCKET_COIN_QA_CREDIT {\"timer\":%u,\"global\":%u,\"serial\":%u,\"collector\":%u,\"authority\":%u,\"request\":%u,\"epoch\":%u,\"area_sequence\":%u,\"authority_sequence\":%u,\"sync\":%u,\"parent\":%u,\"ordinal\":%u,\"value\":%d,\"before\":%.5f,\"after\":%.5f,\"active\":%d,\"result\":%d,\"ordinary\":%d,\"effective\":%d,\"water\":%d}\n",
        gGlobalTimer,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:255,coinQaCredit.serial,e->collector,e->authority,
        e->request,e->epoch,e->areaSequence,e->authoritySequence,e->syncId,e->parent,e->ordinal,e->value,
        before.boost,after.boost,active&&afterActive,result,rocket_boost_mode(),rocket_runtime_boost_mode(),after.water_mode);
}
#define COIN_APPLY_CREDIT(event) coin_qa_apply_credit(event)
#else
#define COIN_APPLY_CREDIT(event) rocket_runtime_collect_coin()
#endif
