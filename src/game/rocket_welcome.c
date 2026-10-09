#include "rocket_welcome.h"
#include "rocket_adapter.h"
#include "save_file.h"
#include "level_update.h"
#include "level_table.h"
#include "ingame_menu.h"
#include "camera.h"
#include "hardcoded.h"
#include "game_init.h"
#include "pc/cliopts.h"
#include "pc/rocket_runtime.h"
#include "pc/character_wheel.h"
#include "pc/network/network.h"
#include "sm64.h"
#include "object_fields.h"
#include <math.h>

extern u8 gSaveFileUsingBackupSlot;

static struct Object *candidate, *speaker;
static s16 candidateSlot, speakerSlot;
static u32 lastFrame;
static unsigned settled;
static int shown;

/* The shipped standalone launcher skips both native intros. Keep only an
 * unused slot's courtyard Lakitu available, without changing the global intro
 * policy for Mario/other characters, existing saves or online sessions. */
int rocket_welcome_pending(void) {
    return gCLIOpts.offline && (gCLIOpts.rocketCar || gCLIOpts.characterNet || gCLIOpts.characterWheel)
        && !gDjuiInMainMenu && gCurrLevelNum==LEVEL_CASTLE_GROUNDS
        && gCurrSaveFileNum>=1 && gCurrSaveFileNum<=NUM_SAVE_FILES
        && !gSaveFileUsingBackupSlot && !save_file_exists(gCurrSaveFileNum-1);
}
void rocket_welcome_reset(struct Object *lakitu) {
    if(candidate==lakitu){candidate=NULL;settled=0;}
    if(speaker==lakitu){speaker=NULL;shown=0;}
}
int rocket_welcome_ready(struct MarioState *m,struct Object *lakitu) {
    RocketSnapshot car;
    if(!rocket_welcome_pending() || !m || m->playerIndex || !m->marioObj || !m->area
       || m->area->index!=1 || m->action!=ACT_IDLE || m->health<0x100 || !m->floor
       || m->heldObj || m->riddenObj || m->heldByObj || m->squishTimer || !m->visibleToObjects
       || sCurrPlayMode!=PLAY_MODE_NORMAL || sDelayedWarpOp!=WARP_OP_NONE || gWarpTransition.isActive || character_wheel_blocks_gameplay()
       || get_dialog_id()!=DIALOG_NONE || !gCamera || gCamera->cutscene
       || !rocket_adapter_car_selected() || !rocket_runtime_snapshot(&car)
       || !car.grounded || car.flipping || car.boosting || !isfinite(car.basis[7]) || car.basis[7]<.9f
       || !isfinite(car.velocity[0]) || !isfinite(car.velocity[1]) || !isfinite(car.velocity[2])
       || fabsf(car.velocity[0])+fabsf(car.velocity[1])+fabsf(car.velocity[2])>30.f
       || m->pos[1]<m->waterLevel+80.f) {
        candidate=NULL;settled=0;return 0;
    }
    if(candidate!=lakitu || candidateSlot!=gCurrSaveFileNum || (u32)(gGlobalTimer-lastFrame)>1)settled=0;
    if(candidate!=lakitu || lastFrame!=gGlobalTimer)++settled;
    candidate=lakitu;candidateSlot=gCurrSaveFileNum;lastFrame=gGlobalTimer;
    return settled>=15; // Half a second on safe, dry ground; repeated draws do not advance it.
}
void rocket_welcome_started(struct Object *lakitu) {
    speaker=lakitu;speakerSlot=gCurrSaveFileNum;shown=0;
}
void rocket_welcome_spawn(struct Object *lakitu) {
    if(speaker!=lakitu || speakerSlot!=gCurrSaveFileNum || !rocket_welcome_pending())return;
    // Native flight waits to turn downward until within 5000 units. The stock
    // bridge-relative origin is over 7700 horizontal units from fresh spawn.
    // Keep the original flight itself, starting nearby and above this player.
    lakitu->oPosX=gMarioStates[0].pos[0]+1200.f;
    lakitu->oPosY=gMarioStates[0].pos[1]+1600.f;
    lakitu->oPosZ=gMarioStates[0].pos[2]-1200.f;
}
void rocket_welcome_observe(struct Object *lakitu) {
    if(speaker==lakitu && speakerSlot==gCurrSaveFileNum && get_dialog_id()==gBehaviorValues.dialogs.LakituIntroDialog)shown=1;
}
void rocket_welcome_finished(struct Object *lakitu) {
    if(speaker!=lakitu)return;
    if(shown && speakerSlot==gCurrSaveFileNum && rocket_welcome_pending()) {
        // Existing native file-exists marker is sufficient: no new save schema,
        // progress bits, stars or settings. Commit only after a displayed dialog.
        save_file_set_flags(SAVE_FLAG_FILE_EXISTS);
        save_file_do_save(gCurrSaveFileNum-1,TRUE);
    }
    speaker=NULL;shown=0;candidate=NULL;settled=0;
}
