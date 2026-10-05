/* Real native health/timer code. Service stubs never grant a cap or change HP. */
#include <assert.h>
#include <stdio.h>
#ifdef ROCKET_NATIVE_SLICE
#include "metal_health_native.inc"
#else
#include "../../../src/game/mario.c"
#endif
struct MarioState gMarioStates[MAX_PLAYERS];
struct MarioState *gMarioState;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS];
u32 gGlobalTimer;
s8 gDebugLevelSelect;
s32 gRumblePakTimer;
f32 gGlobalSoundSource[3];
static int stops,fades;
static int managedCap;
/* Only selects timer ownership. This fixture never creates an online lease;
 * the real grant/replay/expiry authority is exercised by test_wing.c. */
int rocket_caps_managed(const struct MarioState *m){(void)m;return managedCap;}
static u32 visuals;
static unsigned visualUpdates;
void rocket_runtime_set_cap_visuals(uint32_t flags){visuals=flags&MARIO_SPECIAL_CAPS;visualUpdates++;}
void stop_cap_music(void){stops++;}
void fadeout_cap_music(void){fades++;}
void play_sound(s32 sound,f32 *pos){(void)sound;(void)pos;}
u8 is_rumble_finished_and_queue_empty(void){return 1;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;}
int main(void){
    struct Area area={0};struct MarioState *m=&gMarioStates[0];
    m->area=&area;m->health=0x800;m->waterLevel=2000;m->action=ACT_METAL_WATER_STANDING;
    m->flags=MARIO_METAL_CAP|MARIO_CAP_ON_HEAD;m->capTimer=600;
    managedCap=1;update_and_return_cap_flags(m);assert(m->capTimer==600);
    managedCap=0; // The remaining cases exercise the actual offline native clock.
    for(int i=0;i<100;i++)update_mario_health(m);assert(m->health==0x800);
    area.terrainType=TERRAIN_SNOW;update_mario_health(m);assert(m->health==0x800);
    m->action=ACT_METAL_WATER_FALLING;m->input=INPUT_IN_POISON_GAS;update_mario_health(m);assert(m->health==0x800);
    m->hurtCounter=1;update_mario_health(m);assert(m->health==0x7c0&&!m->hurtCounter); // Existing hurt is not erased.
    m->input=0;m->capTimer=2;
    update_and_return_cap_flags(m);assert(m->capTimer==1&&(m->flags&MARIO_METAL_CAP));
    update_and_return_cap_flags(m);assert(!m->capTimer&&!(m->flags&MARIO_SPECIAL_CAPS)&&stops==1);
    m->action=ACT_WATER_IDLE;update_mario_health(m);assert(m->health==0x7bd); // Cold-water oxygen resumes.
    area.terrainType=TERRAIN_GRASS;update_mario_health(m);assert(m->health==0x7bc);
    m->pos[1]=m->waterLevel-140;update_mario_health(m);assert(m->health==0x7d6);
    m->input=INPUT_IN_POISON_GAS;update_mario_health(m);assert(m->health==0x7d2);
    m->flags|=MARIO_METAL_CAP;m->capTimer=70;m->action=ACT_READING_AUTOMATIC_DIALOG;
    update_and_return_cap_flags(m);assert(m->capTimer==70);
    m->capTimer=60;update_and_return_cap_flags(m);assert(m->capTimer==59);
    m->action=ACT_METAL_WATER_WALKING;m->capTimer=61;update_and_return_cap_flags(m);assert(fades==1&&m->capTimer==60);
    unsigned blink=0;for(int i=0;i<59;i++){u32 flags=update_and_return_cap_flags(m);blink+=!(flags&MARIO_METAL_CAP);assert(m->flags&MARIO_METAL_CAP);}
    assert(blink);update_and_return_cap_flags(m);assert(!(m->flags&MARIO_METAL_CAP));
    struct Object object={0};struct MarioBodyState body={0};
    gMarioState=m;m->marioObj=&object;m->marioBodyState=&body;
    m->flags=MARIO_METAL_CAP|MARIO_WING_CAP|MARIO_VANISH_CAP;m->capTimer=100;
    mario_update_hitbox_and_cap_model(m);assert(visuals==MARIO_SPECIAL_CAPS&&m->capTimer==99&&visualUpdates==1);
    m->capTimer=1;mario_update_hitbox_and_cap_model(m);assert(!visuals&&!m->capTimer&&visualUpdates==2);
    struct MarioState remote=*m;remote.playerIndex=1;remote.flags=MARIO_METAL_CAP;remote.capTimer=100;
    gMarioState=&remote;mario_update_hitbox_and_cap_model(&remote);assert(!visuals&&visualUpdates==2);
    puts("PASS native health/cap code: metal oxygen/gas rules, pending hurt, expiry swimming, surface healing, native timer/flicker and shared visual setter ownership");
}
