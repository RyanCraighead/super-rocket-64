/* Actual native interaction handlers, inert audio/camera/world services. */
#include <assert.h>
#include <stdio.h>
#ifdef ROCKET_NATIVE_SLICE
#include "metal_interaction_native.inc"
#else
#include "../../../src/game/interaction.c"
#endif
#include "../../../src/engine/math_util.c"
struct MarioState gMarioStates[MAX_PLAYERS];
struct ServerSettings gServerSettings;
struct LevelValues gLevelValues;
struct BehaviorValues gBehaviorValues;
const BehaviorScript bhvBowser[]={0},bhvKoopaShellUnderwater[]={0},bhvCarrySomething4[]={0};
const BehaviorScript bhvNormalCap[]={0},bhvMetalCap[]={0},bhvWingCap[]={0},bhvVanishCap[]={0};
const BehaviorScript bhvBowserKey[]={0},bhvStarKeyCollectionPuffSpawner[]={0};
u8 gLastCollectedStarOrKey;
s16 gCurrLevelNum,gCurrSaveFileNum=1;
u32 gGlobalTimer;
static struct Object playerObject,enemy;
static struct Surface floorObject;
static int actions,collects,starSaves,starSends,savedIndex;
/* Offline native interaction fixture; real shared-cap host authority is
 * covered by test_wing.c and must not be simulated by granting here. */
int rocket_caps_interact(struct MarioState *m,struct Object *object){(void)m;(void)object;return 0;}
int rocket_runtime_owns_controls(void){return 0;}
int spiderman_adapter_enemy_contact(struct MarioState *m,struct Object *object){(void)m;(void)object;return 0;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;}
void set_camera_shake_from_hit(s16 shake){(void)shake;}
void update_mario_sound_and_camera(struct MarioState *m){(void)m;}
void play_character_sound(struct MarioState *m,enum CharacterSound sound){(void)m;(void)sound;}
void play_sound(s32 sound,f32 *position){(void)sound;(void)position;}
void mario_set_forward_vel(struct MarioState *m,f32 velocity){m->forwardVel=velocity;}
u8 is_player_active(struct MarioState *m){return m==&gMarioStates[0];}
u32 set_mario_action(struct MarioState *m,u32 action,u32 arg){m->action=action;m->actionArg=arg;actions++;return 1;}
s32 drop_and_set_mario_action(struct MarioState *m,u32 action,u32 arg){return set_mario_action(m,action,arg);}
void *virtual_to_segmented(u32 segment,const void *address){(void)segment;return (void *)address;}
void *segmented_to_virtual(const void *address){return (void *)address;}
const BehaviorScript *smlua_override_behavior(const BehaviorScript *behavior){return behavior;}
u32 smlua_get_action_interaction_type(struct MarioState *m){(void)m;return 0;}
bool smlua_call_event_hooks_HOOK_ON_ATTACK_OBJECT(struct MarioState *m,struct Object *object,s32 interaction){(void)m;(void)object;(void)interaction;return false;}
void set_mario_particle_flags(struct MarioState *m,u32 flags,u8 clear){(void)m;(void)flags;(void)clear;}
void network_send_object(struct Object *object){(void)object;}
void network_send_object_reliability(struct Object *object,bool reliable){(void)object;(void)reliable;}
void stop_shell_music(void){}
void drop_queued_background_music(void){}
void fadeout_level_music(s16 frames){(void)frames;}
struct Object *spawn_object(struct Object *parent,s32 model,const BehaviorScript *behavior){(void)parent;(void)model;(void)behavior;return NULL;}
void network_send_collect_star(struct Object *object,s16 coins,s16 star){(void)object;(void)coins;assert(star==5);starSends++;}
void save_file_collect_star_or_key(s16 coins,s16 star,u8 remote){(void)coins;assert(!remote);starSaves++;savedIndex=star;}
s32 save_file_get_total_star_count(s32 file,s32 low,s32 high){(void)file;(void)low;(void)high;return starSaves;}
void save_file_do_save(s32 file,s8 force){assert(file==0&&force);}
void play_cap_music(u16 sequence){(void)sequence;}
void network_send_collect_item(struct Object *object){(void)object;collects++;}
void obj_set_held_state(struct Object *object,const BehaviorScript *behavior){(void)object;(void)behavior;assert(0);}
s32 f32_find_wall_collision(f32 *x,f32 *y,f32 *z,f32 offset,f32 radius){(void)x;(void)y;(void)z;(void)offset;(void)radius;return 0;}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){(void)x;(void)y;(void)z;*floor=&floorObject;return 0;}
static struct MarioState *fresh(void){
    memset(gMarioStates,0,sizeof gMarioStates);memset(&playerObject,0,sizeof playerObject);memset(&enemy,0,sizeof enemy);
    struct MarioState *m=&gMarioStates[0];m->marioObj=&playerObject;m->health=0x880;m->flags=MARIO_CAP_ON_HEAD|MARIO_METAL_CAP;
    m->action=ACT_IDLE;m->waterLevel=-10000;playerObject.hitboxRadius=37;
    enemy.oDamageOrCoinValue=2;enemy.oPosZ=100;gInteractionInvulnerable=0;actions=collects=0;return m;
}
int main(void){
    struct MarioState *m=fresh();assert(interact_damage(m,INTERACT_DAMAGE,&enemy));
    assert(!m->hurtCounter&&actions==1&&(m->action&ACT_FLAG_INVULNERABLE)); // Metal still reacts to impact.
    m=fresh();m->flags&=~MARIO_METAL_CAP;assert(interact_damage(m,INTERACT_DAMAGE,&enemy)&&m->hurtCounter==8);
    m=fresh();assert(!interact_flame(m,INTERACT_FLAME,&enemy)&&!actions&&!m->hurtCounter);
    m=fresh();assert(interact_shock(m,INTERACT_SHOCK,&enemy)&&m->action==ACT_SHOCKED&&!m->hurtCounter);
    m=fresh();m->action=ACT_METAL_WATER_WALKING;
    assert(interact_shock(m,INTERACT_SHOCK,&enemy)&&m->action==ACT_WATER_SHOCKED&&!m->hurtCounter);
    m=fresh();enemy.oInteractionSubtype=INT_SUBTYPE_EATS_MARIO;
    assert(interact_clam_or_bubba(m,INTERACT_CLAM_OR_BUBBA,&enemy)&&m->action==ACT_EATEN_BY_BUBBA);
    m=fresh();m->action=ACT_METAL_WATER_FALLING;
    assert(interact_whirlpool(m,INTERACT_WHIRLPOOL,&enemy)&&m->action==ACT_CAUGHT_IN_WHIRLPOOL);
    m=fresh();m->flags=0;enemy.behavior=bhvMetalCap;gLevelValues.metalCapDuration=600;
    assert(interact_cap(m,INTERACT_CAP,&enemy)&&m->capTimer==600&&(m->flags&MARIO_METAL_CAP));
    assert(m->action==ACT_PUTTING_ON_CAP&&collects==1&&enemy.oInteractStatus==INT_STATUS_INTERACTED);
    m->capTimer=900;assert(interact_cap(m,INTERACT_CAP,&enemy)&&m->capTimer==900); // No shorten on recollect.
    struct MarioState remote=*m;remote.playerIndex=1;remote.flags=0;remote.capTimer=0;
    assert(!interact_cap(&remote,INTERACT_CAP,&enemy)&&!remote.flags&&!remote.capTimer);
    m=fresh();m->flags=0;m->action=ACT_FREEFALL;m->vel[1]=20;enemy.oPosY=100;
    assert(interact_breakable(m,INTERACT_BREAKABLE,&enemy));
    assert(enemy.oInteractStatus&INT_STATUS_WAS_ATTACKED); // Native upward hit opens a tangible cap block.
    m=fresh();assert(!interact_breakable(m,INTERACT_BREAKABLE,&enemy)&&!enemy.oInteractStatus);
    m=fresh();m->action=ACT_METAL_WATER_WALKING;m->capTimer=300;enemy.oBehParams=5u<<24;
    assert(interact_star_or_key(m,INTERACT_STAR_OR_KEY,&enemy)&&m->action==ACT_STAR_DANCE_WATER);
    assert(starSends==1&&starSaves==1&&savedIndex==5&&m->capTimer==1&&m->numStars==1);
    remote=*m;remote.playerIndex=1;assert(!interact_star_or_key(&remote,INTERACT_STAR_OR_KEY,&enemy)&&starSaves==1);
    puts("PASS actual native metal damage/knockback, flame, shock, Bubba/whirlpool exceptions, cap pickup/owner, block attack and underwater star save/expiry");
}
