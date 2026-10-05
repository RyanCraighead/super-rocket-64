/* Compare the car's vertical adaptation directly with native metal falling.
 * Native collision is replaced by an open-water position sink for this test. */
#include <assert.h>
#include <stdio.h>
#include "sm64.h"
u32 perform_water_full_step(struct MarioState *m,Vec3f next);
#include "metal_water_native.inc"
#include "../../../src/engine/math_util.c"
#include "../physics/metal_water.h"
struct Area *gCurrentArea;
s16 gCurrLevelNum,gCurrAreaIndex;
bool smlua_call_event_hooks_HOOK_BEFORE_PHYS_STEP(struct MarioState *m,s32 type,u32 arg,s32 *result){(void)m;(void)type;(void)arg;(void)result;return false;}
bool smlua_call_event_hooks_HOOK_ALLOW_FORCE_WATER_ACTION(struct MarioState *m,bool water,bool *allow){(void)m;(void)water;(void)allow;return false;}
u32 perform_water_full_step(struct MarioState *m,Vec3f next){vec3f_copy(m->pos,next);return WATER_STEP_NONE;}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){(void)x;(void)y;(void)z;(void)floor;assert(0);return 0;}
int main(void){
    struct MarioState m={0};struct Object object={0};struct Surface floor={0};
    m.marioObj=&object;m.waterLevel=10000;m.flags=MARIO_METAL_CAP;m.action=ACT_METAL_WATER_FALLING;
    for(int start=-100;start<=100;start++){
        m.vel[1]=start*.37f;
        float car=m.vel[1]*30.f;
        for(int frame=0;frame<80;frame++){
            stationary_slow_down(&m);
            for(int sub=0;sub<4;sub++)car=rocket_metal_water_velocity(car);
            assert(fabsf(car/30.f-m.vel[1])<.0001f);
        }
    }
    floor.type=SURFACE_FLOWING_WATER;floor.force=0;m.floor=&floor;
    m.pos[0]=m.pos[1]=m.pos[2]=0;m.vel[0]=m.vel[1]=m.vel[2]=0;
    perform_water_step(&m);assert(m.pos[0]==0&&m.pos[1]==0&&m.pos[2]==0);
    // Native expiry/swim resumes the actual flowing-water current.
    m.flags=0;m.action=ACT_WATER_IDLE;
    perform_water_step(&m);assert(m.pos[2]==28);
    // Invulnerable native water actions have their own buoyancy exception;
    // the adapter hands those actions back to native physics.
    m.flags=MARIO_METAL_CAP;m.action=ACT_WATER_SHOCKED;assert(get_buoyancy(&m)==-2);
    puts("PASS 16080 native/car falling comparisons, native metal current resistance, expiry current recovery and invulnerable buoyancy exception");
}
