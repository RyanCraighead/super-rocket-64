/* Actual native classification/force routines linked with inert world services.
 * This is windowless source-level integration, not native gameplay acceptance. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "game/area.h"
#include "game/rocket_environment.h"
#include "game/mario.h"
#include "game/mario_step.h"
#include "game/mario_actions_submerged.h"
#include "pc/lua/smlua_hooks.h"
#include "level_table.h"
#include "trig_tables.inc.c"
#include "environment_native.inc"
struct Area *gCurrentArea;
s16 gCurrLevelNum,gCurrAreaIndex;
u32 gGlobalTimer;
bool gDjuiInMainMenu;
static int checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
bool smlua_call_event_hooks_HOOK_ALLOW_HAZARD_SURFACE(struct MarioState *m,s32 type,bool *allow){(void)m;(void)type;(void)allow;return false;}
bool smlua_call_event_hooks_HOOK_MARIO_OVERRIDE_FLOOR_CLASS(struct MarioState *m,s32 type,s32 *result){(void)m;(void)type;(void)result;return false;}
void apply_vertical_wind(struct MarioState *m);
int main(void){
    struct Area area={0};struct Surface surface={0};struct MarioState m={0};RocketSnapshot pose={0};RocketEnvironment e;
    gCurrentArea=&area;m.area=&area;m.floor=&surface;m.waterLevel=-10000;surface.normal.y=1;
    pose.basis[2]=pose.basis[3]=pose.basis[7]=1;pose.grounded=1;
    const int slippery[]={SURFACE_SLIPPERY,SURFACE_NOISE_SLIPPERY,SURFACE_HARD_SLIPPERY,SURFACE_NO_CAM_COL_SLIPPERY};
    for(unsigned i=0;i<sizeof slippery/sizeof *slippery;i++){surface.type=slippery[i];CHECK(rocket_environment_material(&m,&surface)==1);}
    const int ice[]={SURFACE_ICE,SURFACE_VERY_SLIPPERY,SURFACE_HARD_VERY_SLIPPERY,SURFACE_NOISE_VERY_SLIPPERY_73,SURFACE_NOISE_VERY_SLIPPERY_74,SURFACE_NOISE_VERY_SLIPPERY,SURFACE_NO_CAM_COL_VERY_SLIPPERY};
    for(unsigned i=0;i<sizeof ice/sizeof *ice;i++){surface.type=ice[i];CHECK(rocket_environment_material(&m,&surface)==2);}
    surface.type=SURFACE_DEFAULT;CHECK(rocket_environment_material(&m,&surface)==0);
    area.terrainType=TERRAIN_SLIDE;CHECK(rocket_environment_material(&m,&surface)==2);
    surface.type=SURFACE_NOT_SLIPPERY;CHECK(rocket_environment_material(&m,&surface)==0);
    area.terrainType=0;surface.type=SURFACE_ICE;surface.normal.y=.98f;CHECK(rocket_environment_material(&m,&surface)==6);
    surface.type=SURFACE_SLIPPERY;CHECK(rocket_environment_material(&m,&surface)==1);
    surface.normal.y=.93f;CHECK(rocket_environment_material(&m,&surface)==5);
    surface.type=SURFACE_DEFAULT;CHECK(rocket_environment_material(&m,&surface)==0);
    surface.normal.y=.7f;CHECK(rocket_environment_material(&m,&surface)==4);
    surface.type=SURFACE_HARD_NOT_SLIPPERY;CHECK(rocket_environment_material(&m,&surface)==0);
    // Ground wind follows native idle gusts, heading and moving-speed semantics.
    surface.type=SURFACE_HORIZONTAL_WIND;surface.force=0;gGlobalTimer=0;
    rocket_environment_sample(&m,&pose,&e);CHECK(fabsf(e.drift[2]-96.f)<.001f);
    m.flags=MARIO_METAL_CAP;rocket_environment_sample(&m,&pose,&e);CHECK(fabsf(e.drift[2]-96.f)<.001f);
    pose.velocity[2]=300;rocket_environment_sample(&m,&pose,&e);CHECK(fabsf(e.drift[2]-150.f)<.001f);
    pose.velocity[2]=0;pose.grounded=0;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[2]==0);
    pose.grounded=1;surface.type=SURFACE_SHALLOW_MOVING_QUICKSAND;surface.force=0x100;
    rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[2]==240.f);
    surface.force=-1;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[2]==0);
    surface.type=SURFACE_DEFAULT;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[2]==0);
    pose.grounded=0;
    // Real updraft height/velocity clamping, with native Metal still affected.
    surface.type=SURFACE_VERTICAL_WIND;pose.position[1]=-1500;pose.velocity[1]=0;
    rocket_environment_sample(&m,&pose,&e);CHECK(fabsf(e.acceleration[1]-5625.f)<.01f);
    pose.velocity[1]=2000;rocket_environment_sample(&m,&pose,&e);CHECK(e.acceleration[1]==0);
    pose.position[1]=501;pose.velocity[1]=0;rocket_environment_sample(&m,&pose,&e);CHECK(e.acceleration[1]==0);
    // Native flowing-water table and cap transitions; out of water never currents.
    m.flags=0;m.waterLevel=1000;pose.position[1]=0;surface.type=SURFACE_FLOWING_WATER;
    const float speeds[]={28,12,8,4};
    for(int i=0;i<4;i++){surface.force=i<<8;rocket_environment_sample(&m,&pose,&e);CHECK(fabsf(e.drift[2]-speeds[i]*30)<.01f);}
    surface.force=0x40;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[0]>839&&fabsf(e.drift[2])<.01f);
    m.flags=MARIO_METAL_CAP;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[0]==0&&e.drift[2]==0);
    m.flags=0;surface.force=0x400;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[0]==0&&e.drift[2]==0);
    surface.force=0;pose.position[1]=1000;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[2]==0);
    /* Native swimming keeps currents at the surface (-80), above plunge depth.
     * Ownership actions, not only a depth heuristic, define this transition. */
    m.action=ACT_WATER_IDLE;pose.position[1]=m.waterLevel-80;
    rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[2]==840.f);
    m.action=ACT_METAL_WATER_STANDING;m.flags=MARIO_METAL_CAP;
    rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[2]==0);
    m.action=ACT_IDLE;m.flags=0;
    // Whirlpools use native area/level special cases and stop on Metal/area exit.
    struct Whirlpool whirlpool={0};whirlpool.pos[0]=500;whirlpool.strength=20;area.whirlpools[0]=&whirlpool;
    pose.position[1]=0;surface.type=SURFACE_DEFAULT;rocket_environment_sample(&m,&pose,&e);
    CHECK(e.drift[0]>0);float ordinary=e.drift[0];gCurrLevelNum=LEVEL_DDD;gCurrAreaIndex=2;
    rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[0]>ordinary);
    m.flags=MARIO_METAL_CAP;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[0]==0);
    m.flags=0;area.whirlpools[0]=NULL;rocket_environment_sample(&m,&pose,&e);CHECK(e.drift[0]==0);
    // Sampling leaves authoritative native state untouched.
    struct MarioState before=m;rocket_environment_sample(&m,&pose,&e);CHECK(!memcmp(&before,&m,sizeof m));
    printf("native environment mapping: %d checks passed\n",checks);
    return 0;
}
