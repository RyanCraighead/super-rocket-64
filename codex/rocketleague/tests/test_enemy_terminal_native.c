/* Native attack consumption and next-frame standard actions are extracted from
 * production source. Actual collision/incoming code observes that transition.
 * Reuse only the existing explicit inert world/audio/reward fixture services. */
#define main native_reward_component_main
#include "test_enemy_native.c"
#undef main
#include "../../../src/game/rocket_incoming.c"
#include "../../../src/game/object_collision.c"
#include "enemy_terminal_native.inc"
const BehaviorScript bhvGoomba[]={6},bhvLiveHazard[]={7};
static struct Object carObject;
static int carOwned;
static unsigned checks;
#define CHECK(expression) do { checks++;assert(expression); } while(0)
int rocket_adapter_body_snapshot(struct Object *object,RocketSnapshot *car){
    if(object!=&carObject||!carOwned)return 0;
    memset(car,0,sizeof *car);car->basis[2]=car->basis[3]=car->basis[7]=1;return 1;
}
static void clear_contacts(void){
    carObject.numCollidedObjs=target.numCollidedObjs=0;
    carObject.collidedObjInteractTypes=target.collidedObjInteractTypes=0;
}
static void start(void){
    fresh();memset(&carObject,0,sizeof carObject);carOwned=1;
    carObject.oInteractType=INTERACT_PLAYER;carObject.hitboxRadius=carObject.hurtboxRadius=37;
    carObject.hitboxHeight=160;carObject.header.gfx.activeAreaIndex=-1;carObject.header.gfx.areaIndex=1;
    target.header.gfx.activeAreaIndex=target.header.gfx.areaIndex=1;
    target.header.gfx.scale[0]=target.header.gfx.scale[1]=target.header.gfx.scale[2]=1.5f;
    target.behavior=bhvGoomba;target.oPosX=140;target.oPosZ=90;target.oPosY=20;
    obj_set_hitbox(&target,&sGoombaHitbox);target.oInteractStatus=0;
}
int main(void){
    /* Native attack flag consumption does not itself make the Goomba
     * intangible. This exact gap caused QA9 damage after action101. */
    start();CHECK(detect_object_hitbox_overlap(&carObject,&target));
    CHECK(detect_object_hurtbox_overlap(&carObject,&target));
    target.oInteractStatus=INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|ATTACK_FAST_ATTACK;
    CHECK(obj_handle_attacks(&sGoombaHitbox,GOOMBA_ACT_ATTACKED_MARIO,sGoombaAttackHandlers[0])==ATTACK_FAST_ATTACK);
    CHECK(target.oAction==OBJ_ACT_VERTICAL_KNOCKBACK&&target.oIntangibleTimer==0&&!target.oInteractStatus);
    clear_contacts();
    CHECK(!detect_object_hitbox_overlap(&carObject,&target));
    CHECK(!detect_object_hitbox_overlap(&target,&carObject));
    CHECK(!detect_object_hurtbox_overlap(&carObject,&target));
    CHECK(!carObject.numCollidedObjs&&!carObject.collidedObjInteractTypes);
    CHECK(!obj_update_standard_actions(1.5f));CHECK(target.oIntangibleTimer==-1);
    /* Every known native standard defeat action follows the same deferred
     * intangible rule, including a terminal state received from authority. */
    const int terminal[]={OBJ_ACT_HORIZONTAL_KNOCKBACK,OBJ_ACT_VERTICAL_KNOCKBACK,OBJ_ACT_SQUISHED};
    for(unsigned i=0;i<sizeof terminal/sizeof *terminal;i++){
        start();target.oAction=terminal[i];CHECK(!rocket_incoming_overlap(&carObject,&target,0));
        CHECK(!rocket_incoming_overlap(&carObject,&target,1));
        CHECK(!obj_update_standard_actions(1.5f)&&target.oIntangibleTimer==-1);
        target.oAction=GOOMBA_ACT_WALK;target.oIntangibleTimer=0; // Native lifetime reset / live state.
        CHECK(rocket_incoming_overlap(&carObject,&target,0)==1);
    }
    const int live[]={GOOMBA_ACT_WALK,GOOMBA_ACT_ATTACKED_MARIO,GOOMBA_ACT_JUMP};
    for(unsigned i=0;i<sizeof live/sizeof *live;i++){
        start();target.oAction=live[i];CHECK(detect_object_hitbox_overlap(&carObject,&target));
        CHECK(detect_object_hurtbox_overlap(&carObject,&target));
    }
    start();carOwned=0;target.oAction=OBJ_ACT_VERTICAL_KNOCKBACK;target.oPosX=target.oPosZ=0;
    CHECK(rocket_incoming_overlap(&carObject,&target,0)==-1);
    CHECK(detect_object_hitbox_overlap(&carObject,&target)); // Native Mario fallback unchanged.
    start();target.behavior=bhvLiveHazard;target.oAction=OBJ_ACT_VERTICAL_KNOCKBACK;
    CHECK(detect_object_hitbox_overlap(&carObject,&target)); // Numeric action alone is insufficient.
    start();target.behavior=bhvBobomb;target.oInteractType=INTERACT_GRABBABLE;target.oAction=BOBOMB_ACT_EXPLODE;
    CHECK(detect_object_hitbox_overlap(&carObject,&target));
    start();target.behavior=bhvExplosion;target.oInteractType=INTERACT_DAMAGE;
    target.oAction=OBJ_ACT_VERTICAL_KNOCKBACK;target.oDamageOrCoinValue=2;
    CHECK(detect_object_hitbox_overlap(&carObject,&target));
    CHECK(detect_object_hurtbox_overlap(&carObject,&target)); // Independent explosion stays harmful.
    printf("enemy terminal native: %u attack-transition/collision/standard-action/live/Mario/explosion checks passed\n",checks);
    return 0;
}
