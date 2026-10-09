/* Native course spawn and native timer tail around the real car adapter.
 * Runtime/surface services are inert: this never creates a game/window. */
#define main adapter_regression_main
#include "test_adapter.c"
#undef main
#include "course_table.h"
s16 gCurrCourseNum;
u64 sCapFlickerFrames=0x4444449249255555ULL;
static unsigned stoppedMusic,fadeMusic;
int rocket_caps_managed(const struct MarioState *m){(void)m;return 0;}
void stop_cap_music(void){stoppedMusic++;}
void fadeout_cap_music(void){fadeMusic++;}
#include "native_wing_handoff.inc.c"
int main(void){
    unsigned checks=0;
    for(int car=0;car<2;car++){
        fresh();rocket_adapter_set_selected(car);
        gCurrCourseNum=COURSE_TOTWC;gLevelValues.wingCapDurationTotwc=120;
        set_mario_initial_action(&mario,MARIO_SPAWN_FLYING,0);
        assert(mario.action==ACT_FLYING&&mario.actionArg==2);checks++;
        assert((mario.flags&MARIO_WING_CAP)&&mario.capTimer==120);checks++;
        if(!car){assert(!rocket_adapter_update(&mario)&&mario.action==ACT_FLYING);checks++;continue;}
        step();pose.grounded=0;pose.boost=23;
        for(unsigned time=120;time>0;time--){
            step();assert(mario.action==ACT_FREEFALL&&pose.boost==23&&resets==1);checks++;
            update_and_return_cap_flags(&mario);assert(mario.capTimer==time-1);checks++;
            assert(!!(mario.flags&MARIO_WING_CAP)==(time>1));checks++;
        }
        assert(stoppedMusic==1&&fadeMusic==1);checks++;
        /* A character handoff neither restarts nor consumes a native timer. */
        set_mario_initial_action(&mario,MARIO_SPAWN_FLYING,0);step();
        rocket_adapter_set_selected(0);assert(mario.capTimer==120);checks++;
        mario.action=ACT_FLYING;
        assert(!rocket_adapter_update(&mario)&&mario.action==ACT_FLYING);checks++;
        update_and_return_cap_flags(&mario);assert(mario.capTimer==119);checks++;
        rocket_adapter_set_selected(1);step();assert(mario.capTimer==119);checks++;
    }
    printf("Native Wing course/action/timer handoff: %u checks passed\n",checks);
}
