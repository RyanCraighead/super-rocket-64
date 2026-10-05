/* Native SM64 Wing Cap geometry/materials, attached to the car's rigid basis. */
#include "rocket_wing.h"
#include "character_presentation.h"
#include "sm64.h"
#include "area.h"
#include "mario.h"
#include "level_update.h"
#include "object_helpers.h"
#include "object_fields.h"
#include "behavior_data.h"
#include "model_ids.h"
#include "pc/character_net.h"
#include "pc/rocket_runtime.h"
#include "engine/math_util.h"

static struct Object *toppers[MAX_PLAYERS];
void rocket_wing_topper_clear(unsigned i) {
    if (i >= MAX_PLAYERS) return;
    struct Object *o = toppers[i];
    /* The object pool can be recycled by native area teardown. */
    if (o && o->behavior == bhvStaticObject && o->oBehParams == 0x57494e47) {
        o->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
        obj_mark_for_deletion(o);
    }
    toppers[i] = NULL;
}
void rocket_wing_topper_update(void) {
    for (unsigned i = 0; i < MAX_PLAYERS; ++i) {
        struct MarioState *m = &gMarioStates[i];
        u32 visualFlags = rocket_caps_visual_flags(i);
        RocketSnapshot car;
        int pose = i == 0 ? (rocket_runtime_snapshot(&car) || character_presentation_car_snapshot(&car)) :
            character_net_snapshot(i, &car);
        if (!pose || !rocket_wing_active(i)) { rocket_wing_topper_clear(i); continue; }
        struct Object *o = toppers[i];
        if (!o || !(o->activeFlags & ACTIVE_FLAG_ACTIVE) || o->behavior != bhvStaticObject || o->oBehParams != 0x57494e47) {
            o = spawn_object(m->marioObj, MODEL_MARIOS_WING_CAP, bhvStaticObject);
            toppers[i] = o;
            if (!o) continue;
            o->oBehParams = 0x57494e47;
            o->oInteractType = 0; o->oIntangibleTimer = -1; o->oOpacity = 255;
            o->globalPlayerIndex = m->marioObj->globalPlayerIndex;
            o->oFlags = 0;
        }
        obj_set_model(o, visualFlags & MARIO_METAL_CAP ? MODEL_MARIOS_WINGED_METAL_CAP : MODEL_MARIOS_WING_CAP);
        o->oOpacity = visualFlags & MARIO_VANISH_CAP ? 128 : 255;
        /* SM64 cap +Z is its brim; Rocket basis is forward/right/up. Native
         * cap geo has a 1/4 scale. 1.35 gives a readable hat on the front roof. */
        mtxf_identity(o->transform);
        for (int k = 0; k < 3; ++k) {
            o->transform[0][k] = car.basis[3+k];
            o->transform[1][k] = car.basis[6+k];
            o->transform[2][k] = car.basis[k];
            o->transform[3][k] = car.position[k] + 16.f*ROCKET_HOST_SCALE*car.basis[k] +
                38.f*ROCKET_HOST_SCALE*car.basis[6+k];
            o->header.gfx.pos[k] = o->transform[3][k];
            o->header.gfx.scale[k] = 1.35f;
        }
        o->oPosX = o->header.gfx.pos[0]; o->oPosY = o->header.gfx.pos[1]; o->oPosZ = o->header.gfx.pos[2];
        o->header.gfx.throwMatrix = &o->transform;
        o->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
        /* Same native warning flicker. This never interrupts the boost lease. */
        if (!(visualFlags & MARIO_WING_CAP))
            o->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
    }
}
