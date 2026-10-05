/* Actual collision-list and incoming-query implementations. Only the local
 * runtime pose provider is stubbed; no damage result is manufactured here. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/game/rocket_incoming.c"
#include "../../../src/game/object_collision.c"
#include "../../../src/pc/network/network.h"
enum NetworkType gNetworkType;
static struct Object local,remote,enemy;
static RocketSnapshot pose;
static int owned;
const BehaviorScript bhvBobomb[]={0};
const BehaviorScript bhvGoomba[]={1};
int rocket_adapter_body_snapshot(struct Object *object,RocketSnapshot *out) {
    if(!owned||object!=&local)return 0;
    *out=pose;return 1;
}
static void fresh(void) {
    memset(&local,0,sizeof(local));memset(&remote,0,sizeof(remote));memset(&enemy,0,sizeof(enemy));
    memset(&pose,0,sizeof(pose));pose.basis[2]=pose.basis[3]=pose.basis[7]=1;owned=1;
    local.oInteractType=remote.oInteractType=INTERACT_PLAYER;local.oBehParams=1;
    local.hitboxRadius=local.hurtboxRadius=37;local.hitboxHeight=160;
    // Native Mario survives area unloads: activeAreaIndex is a lifecycle
    // sentinel, while areaIndex tracks his actual current area.
    local.header.gfx.activeAreaIndex=-1;local.header.gfx.areaIndex=1;
    remote=local;remote.oBehParams=2;
    enemy.activeFlags=ACTIVE_FLAG_ACTIVE;enemy.oInteractType=INTERACT_DAMAGE;
    enemy.header.gfx.activeAreaIndex=enemy.header.gfx.areaIndex=1;
    enemy.hitboxRadius=enemy.hurtboxRadius=10;enemy.hitboxHeight=enemy.hurtboxHeight=40;
    enemy.oPosY=20;enemy.oPosZ=150;
}
int main(void) {
    fresh();assert(detect_object_hitbox_overlap(&local,&enemy));
    assert(local.numCollidedObjs==1&&enemy.numCollidedObjs==1);
    assert(local.collidedObjs[0]==&enemy&&enemy.collidedObjs[0]==&local);
    assert(local.collidedObjInteractTypes==INTERACT_DAMAGE);
    assert(!enemy.oInteractStatus&&!enemy.oDamageOrCoinValue);
    assert(detect_object_hurtbox_overlap(&local,&enemy));
    assert(!(enemy.oInteractionSubtype&INT_SUBTYPE_DELAY_INVINCIBILITY));
    fresh();assert(detect_object_hitbox_overlap(&enemy,&local)); // Symmetric hitbox query.
    fresh();enemy.hurtboxRadius=1;assert(detect_object_hitbox_overlap(&local,&enemy));
    assert(!detect_object_hurtbox_overlap(&local,&enemy));
    assert(enemy.oInteractionSubtype&INT_SUBTYPE_DELAY_INVINCIBILITY);
    fresh();enemy.oPosY=85;enemy.oPosZ=0;assert(!detect_object_hitbox_overlap(&local,&enemy));
    fresh();enemy.hitboxDownOffset=70;enemy.oPosY=100;assert(detect_object_hitbox_overlap(&local,&enemy));
    fresh();local.numCollidedObjs=4;assert(!detect_object_hitbox_overlap(&local,&enemy)&&!enemy.numCollidedObjs);
    fresh();enemy.numCollidedObjs=4;assert(!detect_object_hitbox_overlap(&local,&enemy)&&!local.numCollidedObjs);
    fresh();enemy.oIntangibleTimer=-1;assert(!detect_object_hitbox_overlap(&local,&enemy));
    fresh();enemy.activeFlags=0;assert(!detect_object_hitbox_overlap(&local,&enemy));
    fresh();enemy.header.gfx.activeAreaIndex=enemy.header.gfx.areaIndex=2;
    assert(!detect_object_hitbox_overlap(&local,&enemy));
    assert(!detect_object_hitbox_overlap(&enemy,&local));
    assert(!detect_object_hurtbox_overlap(&local,&enemy));
    assert(!local.numCollidedObjs&&!enemy.numCollidedObjs&&!local.collidedObjInteractTypes);
    // Changing Mario's current area must not inherit the previous area's
    // contacts, or require changing his persistent native sentinel.
    fresh();local.header.gfx.areaIndex=2;
    assert(!detect_object_hitbox_overlap(&local,&enemy));
    enemy.header.gfx.activeAreaIndex=enemy.header.gfx.areaIndex=2;
    assert(detect_object_hitbox_overlap(&local,&enemy));
    assert(detect_object_hurtbox_overlap(&local,&enemy));
    assert(local.header.gfx.activeAreaIndex==-1);
    fresh();local.oIntangibleTimer=-1;assert(!detect_object_hitbox_overlap(&local,&enemy));
    const u32 untouched[]={INTERACT_COIN,INTERACT_STAR_OR_KEY,INTERACT_DOOR,INTERACT_PLAYER,INTERACT_GRABBABLE,INTERACT_BREAKABLE};
    for(unsigned i=0;i<sizeof(untouched)/sizeof(*untouched);i++) {
        fresh();enemy.oInteractType=untouched[i];assert(rocket_incoming_overlap(&local,&enemy,0)==-1);
        assert(!detect_object_hitbox_overlap(&local,&enemy));
    }
    fresh();enemy.behavior=bhvBobomb;enemy.oInteractType=INTERACT_GRABBABLE;
    assert(detect_object_hitbox_overlap(&local,&enemy));assert(!enemy.oInteractStatus);
    // Identical owner semantics on host/client; no server-only branch. Other
    // peers' car replicas cannot use this pose even if their centers coincide.
    const enum NetworkType roles[]={NT_NONE,NT_SERVER,NT_CLIENT};
    for(unsigned i=0;i<sizeof(roles)/sizeof(*roles);i++) {
        fresh();gNetworkType=roles[i];
        assert(rocket_incoming_overlap(&remote,&enemy,0)==-1);
        assert(!detect_object_hitbox_overlap(&remote,&enemy));assert(!local.numCollidedObjs);
        assert(detect_object_hitbox_overlap(&local,&enemy));
        assert(local.numCollidedObjs==1&&enemy.numCollidedObjs==1&&remote.numCollidedObjs==0);
    }
    fresh();owned=0;assert(!detect_object_hitbox_overlap(&local,&enemy));
    enemy.oPosZ=40;assert(detect_object_hitbox_overlap(&local,&enemy)); // Native Mario fallback.
    puts("PASS native collision lists: Mario area sentinel/current-area transitions, car body/hurtbox, bounds, local ownership, native fallback and excluded interaction types");
}
