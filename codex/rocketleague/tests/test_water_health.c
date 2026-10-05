/* Execute unmodified native health and cap timers while the car publishes its
 * native submerged actions. No stub changes HP, timer, or native flags. */
#include <assert.h>
#include <stdio.h>
#ifdef ROCKET_NATIVE_SLICE
#include "water_health_native.inc"
#else
#include "../../../src/game/mario.c"
#endif
struct MarioState gMarioStates[MAX_PLAYERS];
s8 gDebugLevelSelect;
s32 gRumblePakTimer;
f32 gGlobalSoundSource[3];
static int stops;
/* Unmanaged offline cap: preserve the actual native timer and health code.
 * Shared online lease authority is covered separately by test_wing.c. */
int rocket_caps_managed(const struct MarioState *m){(void)m;return 0;}
void stop_cap_music(void){stops++;}
void fadeout_cap_music(void){}
void play_sound(s32 sound,f32 *pos){(void)sound;(void)pos;}
u8 is_rumble_finished_and_queue_empty(void){return 1;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;}
int main(void){
    struct Area area={0};struct MarioState *m=&gMarioStates[0];
    m->area=&area;m->health=0x800;m->waterLevel=2000;m->action=ACT_WATER_IDLE;
    for(int i=0;i<90;i++)update_mario_health(m);assert(m->health==0x800-90);
    m->flags=MARIO_WING_CAP|MARIO_CAP_ON_HEAD;m->capTimer=2;
    update_mario_health(m);assert(m->health==0x800-91); // Wing never grants breath immunity
    update_and_return_cap_flags(m);assert(m->capTimer==1);
    update_and_return_cap_flags(m);assert(!m->capTimer&&!(m->flags&MARIO_WING_CAP)&&stops==1);
    m->pos[1]=m->waterLevel-140;s16 hp=m->health;update_mario_health(m);assert(m->health==hp+0x1a);
    area.terrainType=TERRAIN_SNOW;hp=m->health;update_mario_health(m);assert(m->health==hp-3);
    m->action=ACT_METAL_WATER_FALLING;m->flags=MARIO_METAL_CAP|MARIO_CAP_ON_HEAD;
    hp=m->health;update_mario_health(m);assert(m->health==hp); // only native metal effect
    m->hurtCounter=1;update_mario_health(m);assert(m->health==hp-0x40&&!m->hurtCounter);
    m->capTimer=1;update_and_return_cap_flags(m);assert(!(m->flags&MARIO_METAL_CAP)&&stops==2);
    m->action=ACT_WATER_IDLE;hp=m->health;update_mario_health(m);assert(m->health==hp-3);
    area.terrainType=TERRAIN_GRASS;m->pos[1]=0;m->health=0x100;
    update_mario_health(m);assert(m->health==0xff); // native submerged dispatcher owns drowning next frame
    puts("PASS native water health: oxygen, Wing expiry, surface healing, cold water, Metal-only protection, hurt and drowning threshold");
}
