/* Actual QA follow-up body, inert input/capture services, native coin handler. */
#define main original_coin_checks
#include "test_coin_boost.c"
#undef main
#include <stdlib.h>
#include "../../../codex/rocketleague/physics/body_contact.h"
#include "../../../codex/rocketleague/physics/enemy_impact.h"
typedef struct SDL_Window SDL_Window;
u32 gGlobalTimer=1;
struct Object gObjectPool[OBJECT_POOL_CAPACITY];
static unsigned captures,approaches;
static float approachX;
int rocket_runtime_snapshot(RocketSnapshot *s){memset(s,0,sizeof(*s));s->boost=fuel;return owns;}
int rocket_runtime_boost_mode(void){return ROCKET_BOOST_COIN_ONLY;}
int rocket_boost_mode(void){return ROCKET_BOOST_COIN_ONLY;}
uint32_t rocket_caps_active_flags(unsigned index){(void)index;return 0;}
static void capture(SDL_Window *w,const char *p,unsigned ms){(void)w;(void)p;(void)ms;captures++;}
static void combined_qa_steer(const RocketSnapshot *s,float x,float z,float goal,float *t,float *b,float *r){
    (void)s;(void)z;assert(goal==250);approaches++;approachX=x;*t=.65f;*b=*r=0;
}
#include "../../../src/pc/rocket_coin_followup_qa.inc.h"
static RocketSnapshot car;
static float throttle,brake,steer;
static int boost,jump;
static void setup(void){
    gCLIOpts.characterNet=gCLIOpts.loopbackOnly=true;gNetworkAreaLoaded=true;
    gCurrCourseNum=1;gCurrActStarNum=1;gCurrLevelNum=9;gCurrAreaIndex=1;gObjectLists=lists;
    select_peer(0);gMarioStates[0].health=0x880;
    memset(&coinFollowQa,0,sizeof coinFollowQa);memset(gObjectPool,0,sizeof gObjectPool);
    memset(&car,0,sizeof car);car.basis[0]=car.basis[7]=1;car.basis[5]=-1;car.grounded=1;car.boost=40;
    car.position[0]=-100;car.velocity[0]=4500;
    gObjectPool[0].oAction=0;gObjectPool[0].hitboxRadius=72;gObjectPool[0].hitboxHeight=50;
    captures=approaches=0;throttle=.65f;brake=steer=0;boost=1;jump=0;
}
static int step(unsigned ms){return coin_qa_followup(NULL,"unused",ms,&car,1,&gObjectPool[0],1,420,&throttle,&brake,&steer,&boost,&jump);}
static void loot(unsigned slot,u32 parent,u32 ordinal){
    struct Object *o=&gObjectPool[slot];o->activeFlags=ACTIVE_FLAG_ACTIVE;o->oInteractType=INTERACT_COIN;
    o->coinBoostParent=parent;o->coinBoostOrdinal=ordinal;o->header.gfx.activeAreaIndex=1;
    o->behavior=yellow;o->oDamageOrCoinValue=1;o->oPosX=512;o->oIntangibleTimer=1;
}
int main(void){
    assert(setenv("SM64_ROCKET_QA_COIN_FOLLOWUP","1",1)==0);
    setup();assert(coin_qa_followup_enabled(5)&&!coin_qa_followup_enabled(4));
    car.position[0]=-1000;assert(!step(1000)&&boost==1&&brake==0&&!coinFollowQa.began);
    car.position[0]=-100;car.velocity[0]=4399;assert(!step(1033)&&boost==1&&!coinFollowQa.began);
    car.velocity[0]=4500;assert(!step(1066)&&!boost&&brake==1&&!coinFollowQa.began);
    puts("PASS driver pre-brake requires actual supersonic chassis overlap, never proximity or lowered threshold");
    gObjectPool[0].oAction=OBJ_ACT_VERTICAL_KNOCKBACK;assert(!step(1100)&&coinFollowQa.parent==420);
    loot(1,420,1);loot(2,421,1);assert(!step(1133)&&!captures&&!approaches);
    gObjectPool[1].oIntangibleTimer=0;assert(!step(1166)&&captures==1&&!approaches&&brake==1);
    car.velocity[0]=0;
    struct Object saved[3];memcpy(saved,gObjectPool,sizeof saved);RocketSnapshot savedCar=car;
    assert(!step(1200)&&approaches==1&&approachX==512);
    assert(!memcmp(saved,gObjectPool,sizeof saved)&&!memcmp(&savedCar,&car,sizeof car));
    puts("PASS driver preserves actors/physics, waits for native tangibility, ignores unrelated coins, brakes before approach");
    interact_coin(&gMarioStates[0],INTERACT_COIN,&gObjectPool[1]);
    assert(gMarioStates[0].numCoins==1&&fuel==45&&creditCount==1);
    car.boost=fuel;assert(!step(1300));assert(step(3501)==1&&!strcmp(coinFollowQa.reason,"reward_collected"));
    assert(captures==1&&!boost&&!jump);
    puts("PASS actual native pickup/credit opens completion only after the bounded neutral observation");
    setup();gObjectPool[0].oAction=OBJ_ACT_VERTICAL_KNOCKBACK;loot(1,420,1);loot(2,420,2);
    assert(step(1000)==-1&&!strcmp(coinFollowQa.reason,"multiple_native_rewards"));
    setup();gObjectPool[0].oAction=OBJ_ACT_VERTICAL_KNOCKBACK;assert(!step(1000));epoch++;
    assert(step(1033)==-1&&!strcmp(coinFollowQa.reason,"runtime_epoch_changed"));
    setup();gObjectPool[0].oAction=OBJ_ACT_VERTICAL_KNOCKBACK;assert(!step(1000));
    assert(step(26001)==-1&&!strcmp(coinFollowQa.reason,"reward_pickup_timeout"));
    puts("PASS duplicate reward, epoch change and timeout fail closed without awards");
    return 0;
}
