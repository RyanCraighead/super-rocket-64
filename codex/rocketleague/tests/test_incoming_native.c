/* Actual native interaction handlers. Audio/camera/world services are inert
 * fixture stubs. This is a source-level test, not native gameplay evidence. */
#include <assert.h>
#include <stdio.h>
#include "../../../src/game/interaction.c"
#include "../../../src/game/rocket_incoming.c"
#include "../../../src/game/object_collision.c"

struct MarioState gMarioStates[MAX_PLAYERS];
struct ServerSettings gServerSettings;
struct LevelValues gLevelValues;
const BehaviorScript bhvBowser[]={0},bhvKoopaShellUnderwater[]={0},bhvCarrySomething4[]={0};
const BehaviorScript bhvBobomb[]={0};
const BehaviorScript bhvGoomba[]={1};
u32 gGlobalTimer;
static struct Object playerObject,enemy;
static struct Surface floorObject;
static int actions,sends;
#ifdef ROCKET_BULLY_REAL_TEST
static RocketSnapshot bullyCar;
static int bullyCarActive=1,bullyRingFloor;
static int bullyFloorMode,bullySideWall;
static float bullyWallZ;
static struct Surface lavaFloor,bullyWall;
#endif
/* This fixture exercises ordinary damage/grabs; real boss ownership has its own suite. */
int boss_net_managed(const struct Object *object){(void)object;return 0;}
int boss_net_simulates(const struct Object *object){(void)object;return 0;}
int rocket_adapter_body_snapshot(struct Object *object,RocketSnapshot *car){
    if(object!=&playerObject||gMarioStates[0].action!=ACT_IDLE)return 0;
#ifdef ROCKET_BULLY_REAL_TEST
    if(!bullyCarActive)return 0;
    *car=bullyCar;return 1;
#endif
    memset(car,0,sizeof(*car));car->basis[2]=car->basis[3]=car->basis[7]=1;return 1;
}
int spiderman_adapter_enemy_contact(struct MarioState *m,struct Object *object){(void)m;(void)object;return 0;}
void queue_rumble_data_mario(struct MarioState *m,s16 a,s16 b){(void)m;(void)a;(void)b;}
void set_camera_shake_from_hit(s16 shake){(void)shake;}
void set_camera_mode(struct Camera *camera,s16 mode,s16 frames){(void)camera;(void)mode;(void)frames;}
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
void network_send_object(struct Object *object){(void)object;sends++;}
void network_send_object_reliability(struct Object *object,bool reliable){(void)object;(void)reliable;sends++;}
void stop_shell_music(void){}
void obj_set_held_state(struct Object *object,const BehaviorScript *behavior){(void)object;(void)behavior;assert(0);}
#ifdef ROCKET_BULLY_REAL_TEST
s32 find_wall_collisions(struct WallCollisionData *data){
    if(data->z<=bullyWallZ)return 0;
    data->z=bullyWallZ;data->walls[0]=&bullyWall;data->numWalls=1;
    if(bullySideWall)data->x+=1000; // Reject an excessive corner projection.
    return 1;
}
#endif
s32 f32_find_wall_collision(f32 *x,f32 *y,f32 *z,f32 offset,f32 radius){
#ifdef ROCKET_BULLY_REAL_TEST
    struct WallCollisionData data={.x=*x,.y=*y,.z=*z,.offsetY=offset,.radius=radius};
    int hit=find_wall_collisions(&data);*x=data.x;*y=data.y;*z=data.z;return hit;
#else
    (void)x;(void)y;(void)z;(void)offset;(void)radius;return 0;
#endif
}
f32 find_floor(f32 x,f32 y,f32 z,struct Surface **floor){
    (void)x;(void)y;(void)z;
#ifdef ROCKET_BULLY_REAL_TEST
    if(bullyFloorMode==1){*floor=NULL;return -11000;}
    if(bullyFloorMode==2){*floor=&floorObject;return 100;}
    if(bullyRingFloor&&z>=200){*floor=&lavaFloor;return 0;}
#endif
    *floor=&floorObject;return 0;
}

static struct MarioState *fresh(void){
    memset(gMarioStates,0,sizeof(gMarioStates));memset(&playerObject,0,sizeof(playerObject));memset(&enemy,0,sizeof(enemy));
    struct MarioState *m=&gMarioStates[0];m->marioObj=&playerObject;m->health=0x880;m->flags=MARIO_CAP_ON_HEAD;
    m->action=ACT_IDLE;m->waterLevel=-10000;playerObject.hitboxRadius=37;
    playerObject.oInteractType=INTERACT_PLAYER;playerObject.oBehParams=1;
    playerObject.header.gfx.activeAreaIndex=-1;playerObject.header.gfx.areaIndex=1;
    enemy.header.gfx.activeAreaIndex=enemy.header.gfx.areaIndex=1;
    enemy.oInteractType=INTERACT_DAMAGE;enemy.oDamageOrCoinValue=2;enemy.oPosZ=100;
    enemy.oPosY=20;enemy.hitboxRadius=enemy.hurtboxRadius=10;
    enemy.hitboxHeight=enemy.hurtboxHeight=40;enemy.activeFlags=ACTIVE_FLAG_ACTIVE;
    gInteractionInvulnerable=0;actions=sends=0;return m;
}
int main(void){
    struct MarioState *m=fresh();assert(detect_object_hitbox_overlap(&playerObject,&enemy));
    assert(detect_object_hurtbox_overlap(&playerObject,&enemy));
    assert(mario_get_collided_object(m,INTERACT_DAMAGE)==&enemy);
    assert(interact_damage(m,INTERACT_DAMAGE,&enemy));
    assert(m->hurtCounter==8&&actions==1&&(m->action&ACT_FLAG_INVULNERABLE));
    assert(enemy.oInteractStatus==(INT_STATUS_INTERACTED|INT_STATUS_ATTACKED_MARIO));
    // The next native interaction pass derives invulnerability from this action.
    gInteractionInvulnerable=(m->action&ACT_FLAG_INVULNERABLE)||m->invincTimer!=0;
    assert(!interact_damage(m,INTERACT_DAMAGE,&enemy));assert(m->hurtCounter==8&&actions==1);
    // A regular Goomba's actual scaled hitbox/hurtbox reaches the side of the
    // chassis outside Mario's capsule. Native area=-1 must not erase intake.
    m=fresh();enemy.oInteractType=INTERACT_BOUNCE_TOP;enemy.oDamageOrCoinValue=1;
    enemy.behavior=bhvGoomba;
    enemy.hitboxRadius=108;enemy.hitboxHeight=75;
    enemy.hurtboxRadius=63;enemy.hurtboxHeight=60;
    enemy.oPosX=140;enemy.oPosZ=90;
    assert(hypotf(enemy.oPosX,enemy.oPosZ)>playerObject.hitboxRadius+enemy.hitboxRadius);
    assert(detect_object_hitbox_overlap(&playerObject,&enemy));
    assert(detect_object_hurtbox_overlap(&playerObject,&enemy));
    assert(mario_get_collided_object(m,INTERACT_BOUNCE_TOP)==&enemy);
    assert(interact_bounce_top(m,INTERACT_BOUNCE_TOP,&enemy));
    assert(m->hurtCounter==4&&actions==1&&(m->action&ACT_FLAG_INVULNERABLE));
    assert(m->interactObj==&enemy&&enemy.oInteractStatus==(INT_STATUS_INTERACTED|INT_STATUS_ATTACKED_MARIO));
    m=fresh();m->invincTimer=30;gInteractionInvulnerable=m->invincTimer!=0;
    assert(!interact_damage(m,INTERACT_DAMAGE,&enemy)&&!m->hurtCounter&&!actions);
    m=fresh();m->flags|=MARIO_VANISH_CAP;assert(!interact_damage(m,INTERACT_DAMAGE,&enemy)&&!m->hurtCounter);
    m=fresh();m->flags|=MARIO_METAL_CAP;assert(interact_damage(m,INTERACT_DAMAGE,&enemy)&&!m->hurtCounter);
    m=fresh();enemy.oInteractionSubtype=INT_SUBTYPE_DELAY_INVINCIBILITY;
    assert(!interact_damage(m,INTERACT_DAMAGE,&enemy)&&!m->hurtCounter);
    m=fresh();m->flags=0;assert(interact_damage(m,INTERACT_DAMAGE,&enemy)&&m->hurtCounter==12);
    m=fresh();assert(interact_flame(m,INTERACT_FLAME,&enemy));assert(m->action==ACT_BURNING_JUMP);
    m=fresh();m->flags|=MARIO_METAL_CAP;assert(!interact_flame(m,INTERACT_FLAME,&enemy));
    m=fresh();assert(interact_shock(m,INTERACT_SHOCK,&enemy));assert(m->action==ACT_SHOCKED&&m->hurtCounter==8);
    m=fresh();enemy.oInteractType=INTERACT_GRABBABLE;enemy.oDamageOrCoinValue=0;
    enemy.oInteractionSubtype=INT_SUBTYPE_KICKABLE;enemy.hitboxRadius=65;enemy.oPosZ=50;
    assert(!interact_grabbable(m,INTERACT_GRABBABLE,&enemy));
    assert(!m->hurtCounter&&!enemy.oInteractStatus&&!actions&&!sends&&m->pos[2]<0); // Ordinary touch pushes, never damages/explodes.
    m->playerIndex=1;assert(!interact_grabbable(m,INTERACT_GRABBABLE,&enemy));assert(!sends);
    puts("PASS native incoming handlers: native-area Goomba chassis damage, damage/action, repeat immunity, caps, hurtbox delay, fire/shock and zero-damage Bob-omb touch");
}
