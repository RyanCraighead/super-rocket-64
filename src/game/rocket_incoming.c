#include "rocket_incoming.h"
#include "rocket_adapter.h"
#include "sm64.h"
#include "interaction.h"
#include "object_constants.h"
#include "object_fields.h"
#include "behavior_data.h"
#include "../../codex/rocketleague/physics/body_contact.h"

static int incoming_type(const struct Object *object) {
    /* Pickups, doors, players, platforms and arbitrary grabbables keep their
     * native reach. Bob-ombs are included as contacts, not invented damage:
     * their native hitbox has damage=0; their separate explosion has damage=2. */
    const u32 hazards=INTERACT_FLAME|INTERACT_SNUFIT_BULLET|INTERACT_CLAM_OR_BUBBA|
        INTERACT_BULLY|INTERACT_SHOCK|INTERACT_BOUNCE_TOP2|INTERACT_MR_BLIZZARD|
        INTERACT_HIT_FROM_BELOW|INTERACT_BOUNCE_TOP|INTERACT_DAMAGE|INTERACT_KOOPA|
        INTERACT_SPINY_WALKING;
    return !!(object->oInteractType&hazards)||
        (object->behavior==bhvBobomb&&object->oInteractType==INTERACT_GRABBABLE);
}

int rocket_incoming_overlap(struct Object *a, struct Object *b, int hurtbox) {
    if (!a||!b) return -1;
    RocketSnapshot car;
    if (!incoming_type(b)||!rocket_adapter_body_snapshot(a,&car)) return -1;
    /* Native Goomba attack consumption sets a standard defeat action and
     * clears interact status before its next behavior update makes it
     * intangible. Exclude that confirmed terminal frame from car intake too;
     * otherwise the next collision pass damages an idle car before the native
     * update catches up. Live enemies and independent explosions keep intake. */
    if(b->behavior==bhvGoomba&&(b->oAction==OBJ_ACT_HORIZONTAL_KNOCKBACK||
       b->oAction==OBJ_ACT_VERTICAL_KNOCKBACK||b->oAction==OBJ_ACT_SQUISHED))return 0;
    if(b->oInteractType==INTERACT_BULLY&&(b->oAction==BULLY_ACT_LAVA_DEATH||
       b->oAction==BULLY_ACT_DEATH_PLANE_DEATH||b->oSyncDeath))return 0;
    /* Mario's activeAreaIndex is the persistent-object sentinel (-1), not
     * his location. Native area entry/change updates areaIndex instead. */
    if (!(b->activeFlags&ACTIVE_FLAG_ACTIVE)||a->oIntangibleTimer||b->oIntangibleTimer||
        a->header.gfx.areaIndex!=b->header.gfx.activeAreaIndex) return 0;
    float position[3]={b->oPosX,b->oPosY-b->hitboxDownOffset,b->oPosZ};
    return rocket_body_overlaps_cylinder(&car,position,
        hurtbox?b->hurtboxRadius:b->hitboxRadius,hurtbox?b->hurtboxHeight:b->hitboxHeight);
}
