/* Actual native Bob-omb behavior, with explicit contact/environment stubs. */
#include <assert.h>
#include <stdio.h>
#include "../../../src/game/obj_behaviors.c"
struct Object *gCurrentObject;
struct MarioState gMarioStates[MAX_PLAYERS];
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal,*gNetworkPlayerServer;
struct ServerSettings gServerSettings;
enum NetworkType gNetworkType=NT_NONE;
static struct Object bomb,player;
static int attacked;
/* This fixture isolates native behavior; real outgoing policy/authority is
 * exercised in test_enemy_host.c. Keep its hook inert for ordinary touch. */
int rocket_enemy_attack(struct Object *object){(void)object;return 0;}
int rocket_contacts_bobomb_yaw(struct Object *object,s16 *yaw){(void)object;(void)yaw;return 0;}
void obj_set_hitbox(struct Object *object,struct ObjectHitbox *hitbox){
    object->oInteractType=hitbox->interactType;
    object->oDamageOrCoinValue=hitbox->damageOrCoinValue;
}
f32 dist_between_objects(struct Object *a,struct Object *b){(void)a;(void)b;return 1;}
s32 obj_attack_collided_from_other_object(struct Object *object){(void)object;return attacked;}
int main(void){
    memset(&bomb,0,sizeof(bomb));gCurrentObject=&bomb;
    gMarioStates[0].marioObj=&player;gMarioStates[0].visibleToObjects=1;
    bomb.oAction=BOBOMB_ACT_CHASE_MARIO;
    bobomb_check_interactions();
    assert(bomb.oInteractType==INTERACT_GRABBABLE&&bomb.oDamageOrCoinValue==0);
    assert(sBobombHitbox.radius==65&&sBobombHitbox.height==113&&!sBobombHitbox.hurtboxRadius);
    assert(bomb.oAction==BOBOMB_ACT_CHASE_MARIO&&!bomb.oInteractStatus);
    // An actual kick/launch and explicit touched-explosion status are distinct.
    bomb.oInteractStatus=INT_STATUS_INTERACTED|INT_STATUS_MARIO_UNK1;
    bobomb_check_interactions();assert(bomb.oAction==BOBOMB_ACT_LAUNCHED&&bomb.oForwardVel==25&&bomb.oVelY==30);
    bomb.oInteractStatus=INT_STATUS_INTERACTED|INT_STATUS_TOUCHED_BOB_OMB;
    bobomb_check_interactions();assert(bomb.oAction==BOBOMB_ACT_EXPLODE&&!bomb.oInteractStatus);
    puts("PASS native Bob-omb source: zero touch damage, ordinary chase preserved, explicit launch/explosion states unchanged");
}
