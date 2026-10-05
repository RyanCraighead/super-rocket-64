#include <math.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "dialog_ids.h"
#include "area.h"
#include "ingame_menu.h"
#include "level_update.h"
#include "mario.h"
#include "characters.h"
#include "engine/math_util.h"
#include "pc/cliopts.h"
#include "pc/network/network.h"
#include "pc/djui/djui_panel.h"
#include "pc/djui/djui_chat_box.h"
#include "pc/djui/djui_console.h"
#include "pc/oot_link_runtime.h"
#include "pc/bm64_runtime.h"
#include "pc/bk_runtime.h"
#include "pc/spiderman_runtime.h"
#include "pc/thps_runtime.h"
#include "oot_link_adapter.h"
#include "bm64_adapter.h"
#include "bk_adapter.h"
#include "spiderman_adapter.h"
#include "thps_adapter.h"
#include "rocket_adapter.h"
#include "pc/rocket_runtime.h"
#include "character_switch.h"

extern s32 gDialogID;
static int sInitialized;
static int sAvailable[CHARACTER_COUNT];
static int sRequested[CHARACTER_COUNT];
static enum CharacterSwitchId sActive = CHARACTER_MARIO;
static CharacterSwitchSnapshot sSnapshot;
static int sPending = -1;
static enum CharacterSwitchId sPendingFrom;
static struct Area *sPendingArea;
static struct Object *sPendingObject;
static s16 sPendingLevel;
static int sPendingOnline;
static u16 sPendingAreaSequence;
static u8 sPendingGlobal;
static const char *sNames[CHARACTER_COUNT] = {
    "Mario", "Link", "Bomberman", "Banjo & Kazooie", "Spider-Man", "Tony Hawk", "Octane"
};

static int valid_id(enum CharacterSwitchId id) { return id >= 0 && id < CHARACTER_COUNT; }
static void select_adapters(void) {
    /* Each deselection restores native visibility and discards only that
     * controller's transient attacks, effects, held bomb and source clocks. */
    oot_link_adapter_set_selected(sActive == CHARACTER_LINK);
    bm64_adapter_set_selected(sActive == CHARACTER_BOMBERMAN);
    bk_adapter_set_selected(sActive == CHARACTER_BANJO);
    spiderman_adapter_set_selected(sActive == CHARACTER_SPIDERMAN);
    thps_adapter_set_selected(sActive == CHARACTER_TONY);
    rocket_adapter_set_selected(sActive == CHARACTER_OCTANE);
}
void character_switch_init(void) {
    sInitialized = 1;
    character_switch_cancel_pending();
    sActive = CHARACTER_MARIO;
    memset(&sSnapshot, 0, sizeof sSnapshot);
    sSnapshot.previous = sSnapshot.active = CHARACTER_MARIO;
    sRequested[CHARACTER_MARIO] = sAvailable[CHARACTER_MARIO] = 1;
    sRequested[CHARACTER_LINK] = gCLIOpts.ootLink;
    sRequested[CHARACTER_BOMBERMAN] = gCLIOpts.bm64Bomberman;
    sRequested[CHARACTER_BANJO] = gCLIOpts.bkDuo;
    sRequested[CHARACTER_SPIDERMAN] = gCLIOpts.spidermanOriginal;
    sRequested[CHARACTER_TONY] = gCLIOpts.thpsOriginal;
    sRequested[CHARACTER_OCTANE] = gCLIOpts.rocketCar || character_switch_online();
    sAvailable[CHARACTER_LINK] = gCLIOpts.ootLink && oot_link_runtime_enabled();
    sAvailable[CHARACTER_BOMBERMAN] = gCLIOpts.bm64Bomberman && bm64_runtime_enabled();
    sAvailable[CHARACTER_BANJO] = gCLIOpts.bkDuo && bk_runtime_enabled();
    sAvailable[CHARACTER_SPIDERMAN] = gCLIOpts.spidermanOriginal && spiderman_runtime_enabled();
    sAvailable[CHARACTER_TONY] = gCLIOpts.thpsOriginal && thps_runtime_enabled();
    sAvailable[CHARACTER_OCTANE] = sRequested[CHARACTER_OCTANE] && rocket_runtime_enabled();
    if (gCLIOpts.rocketCar && sAvailable[CHARACTER_OCTANE]) sActive = CHARACTER_OCTANE;
    sSnapshot.previous = sSnapshot.active = sActive;
    if (gCLIOpts.characterWheel) select_adapters();
}
int character_switch_online(void) {
    return gCLIOpts.characterWheel && gCLIOpts.characterNet && !gCLIOpts.offline &&
        (gCLIOpts.network == NT_SERVER || gCLIOpts.network == NT_CLIENT) && !gCLIOpts.coopnet;
}
int character_switch_enabled(void) {
    return sInitialized && gCLIOpts.characterWheel &&
        gCLIOpts.disableMods && !gCLIOpts.enabledModsCount && !gCLIOpts.headless &&
        ((gCLIOpts.offline && gCLIOpts.network == NT_NONE && !gCLIOpts.coopnet) || character_switch_online());
}
int character_switch_available(enum CharacterSwitchId id) {
    if (!valid_id(id) || !sInitialized || !sAvailable[id]) return 0;
    if (character_switch_online() && id != CHARACTER_MARIO && id != CHARACTER_OCTANE) return 0;
    switch (id) {
        case CHARACTER_LINK: return oot_link_runtime_enabled();
        case CHARACTER_BOMBERMAN: return bm64_runtime_enabled();
        case CHARACTER_BANJO: return bk_runtime_enabled();
        case CHARACTER_SPIDERMAN: return spiderman_runtime_enabled();
        case CHARACTER_TONY: return thps_runtime_enabled();
        case CHARACTER_OCTANE: return rocket_runtime_enabled();
        default: return id == CHARACTER_MARIO;
    }
}
const char *character_switch_reason(enum CharacterSwitchId id) {
    if (!valid_id(id)) return "Unknown character";
    if (!sInitialized) return "Game is loading";
    if (character_switch_online() && id != CHARACTER_MARIO && id != CHARACTER_OCTANE)
        return "Online switching supports Mario and Octane only";
    if (character_switch_available(id)) return NULL;
    if (sAvailable[id]) return "Original renderer unavailable; restart the game";
    return sRequested[id] ? "Original assets failed validation" : "Original assets not supplied";
}
const char *character_switch_name(enum CharacterSwitchId id) {
    return valid_id(id) ? sNames[id] : "Unknown";
}
enum CharacterSwitchId character_switch_active(void) { return sActive; }
int character_switch_accepts(enum CharacterSwitchId id) {
    return valid_id(id) && (!gCLIOpts.characterWheel || (character_switch_enabled() && id == sActive));
}
int character_switch_native_model(int player_index, int configured_model) {
    return player_index == 0 && character_switch_enabled() ? CT_MARIO : configured_model;
}
static int ordinary_action(u32 action) {
    switch (action) {
        case ACT_IDLE: case ACT_WALKING: case ACT_DECELERATING:
        case ACT_BRAKING: case ACT_BRAKING_STOP: case ACT_TURNING_AROUND:
        case ACT_FINISH_TURNING_AROUND: case ACT_FREEFALL_LAND:
        case ACT_FREEFALL_LAND_STOP: case ACT_JUMP_LAND_STOP:
            return 1;
        default: return 0;
    }
}
static int unsafe_surface(s16 type) {
    return SURFACE_IS_QUICKSAND(type) || SURFACE_IS_PAINTING_WARP(type) ||
        (type >= SURFACE_INSTANT_WARP_1B && type <= SURFACE_INSTANT_WARP_1E) ||
        type == SURFACE_BURNING || type == SURFACE_DEATH_PLANE ||
        type == SURFACE_WARP || type == SURFACE_WOBBLING_WARP ||
        type == SURFACE_VERTICAL_WIND || type == SURFACE_LOOK_UP_WARP;
}
const char *character_switch_can_open(void) {
    struct MarioState *m = &gMarioStates[0];
    if (!character_switch_enabled()) return "Character wheel is not enabled";
    if (sCurrPlayMode != PLAY_MODE_NORMAL || gDjuiInMainMenu || gDjuiInPlayerMenu ||
        djui_panel_is_active() || gDjuiChatBoxFocus || gDjuiConsoleFocus || 0 || gMenuMode != -1 || gDialogID != DIALOG_NONE)
        return "Close the menu or dialogue first";
    if (gWarpTransition.isActive || sWarpDest.type != WARP_TYPE_NOT_WARPING ||
        sDelayedWarpOp != WARP_OP_NONE || sTransitionTimer || gCurrCreditsEntry)
        return "Wait for the transition to finish";
    if (!m->marioObj || !m->controller || !m->area || !m->floor || m->playerIndex != 0)
        return "Enter a level first";
    if (character_switch_online() &&
        (gNetworkType == NT_NONE || !gNetworkAreaLoaded || gNetworkAreaSyncing || !gNetworkPlayerLocal ||
         !gNetworkPlayerLocal->connected || !gNetworkPlayerLocal->currLevelSyncValid ||
         !gNetworkPlayerLocal->currAreaSyncValid ||
         gNetworkPlayerLocal->currLevelNum != gCurrLevelNum ||
         gNetworkPlayerLocal->currAreaIndex != m->area->index))
        return "Wait for online area synchronization";
    if (m->freeze || m->health < 0x100 || m->hurtCounter || m->healCounter ||
        m->squishTimer || m->quicksandDepth > 0 || (m->input & INPUT_SQUISHED))
        return "Wait until the character is safe";
    if (m->heldObj || m->riddenObj || m->heldByObj)
        return "Release the held or ridden object first";
    if (!ordinary_action(m->action)) return "Land and finish the current action first";
    for (int k = 0; k < 3; ++k) {
        if (!isfinite(m->pos[k]) || !isfinite(m->vel[k])) return "Character position is unavailable";
    }
    /* Octane mirrors its chassis center; other adapters mirror feet. */
    float clearance = m->pos[1] - m->floorHeight;
    int unsafeHeight = sActive == CHARACTER_OCTANE ? (clearance < -8.f || clearance > 70.f) : fabsf(clearance) > 8.f;
    if (!isfinite(m->floorHeight) || !isfinite(m->floor->normal.y) ||
        m->floor->normal.y < 0.9f || unsafeHeight ||
        fabsf(m->vel[1]) > 4.0f || m->pos[1] < m->waterLevel || unsafe_surface(m->floor->type))
        return "Stand on safe, dry, level ground";
    if (sActive == CHARACTER_SPIDERMAN) return spiderman_adapter_switch_reason();
    if (sActive == CHARACTER_TONY) return thps_adapter_switch_reason();
    if (sActive == CHARACTER_OCTANE) return rocket_adapter_switch_reason();
    return NULL;
}
int character_switch_prepare_open(void) {
    character_switch_cancel_pending();
    if (character_switch_can_open()) return 0;
    /* Offline pauses the world; online only blocks local controls. Consume
     * charge/attack input history so release or cancel cannot create an edge. */
    thps_adapter_pause_inputs();
    rocket_runtime_interrupt();
    if (sActive == CHARACTER_LINK) oot_link_runtime_pause();
    return 1;
}
int character_switch_commit(enum CharacterSwitchId id) {
    character_switch_cancel_pending();
    if (!character_switch_available(id) || character_switch_can_open()) return 0;
    if (id == sActive) return 1; /* Reselecting never refills a source inventory. */
    struct MarioState *m = &gMarioStates[0];
    sSnapshot.previous = sActive;
    vec3f_copy(sSnapshot.before_position, m->pos);
    sSnapshot.before_health = m->health;
    /* Suspension clears the outgoing render submission before a new adapter
     * may acquire ownership. Asset buffers remain loaded for the whole run. */
    oot_link_adapter_suspend();
    bm64_adapter_suspend();
    bk_adapter_suspend();
    spiderman_adapter_suspend();
    thps_adapter_suspend();
    rocket_adapter_suspend();
    /* A character handoff retires old physical contacts and pending grants,
     * but retains the same world/tank and all native cap/coin ledgers. */
    if (character_switch_online()) rocket_runtime_selection_changed();
    sActive = id;
    select_adapters();
    vec3f_set(m->vel, 0, 0, 0);
    m->forwardVel = m->slideVelX = m->slideVelZ = 0;
    m->intendedMag = 0;
    m->input = 0;
    m->flags &= ~(MARIO_PUNCHING | MARIO_KICKING | MARIO_TRIPPING);
    m->wallKickTimer = m->doubleJumpTimer = 0;
    set_mario_action(m, ACT_IDLE, 0);
    sSnapshot.active = sActive;
    vec3f_copy(sSnapshot.after_position, m->pos);
    sSnapshot.after_health = m->health;
    ++sSnapshot.commits;
    return 1;
}
int character_switch_pending(void) { return sPending; }
void character_switch_cancel_pending(void) {
    sPending = -1;
    sPendingArea = NULL;
    sPendingObject = NULL;
}
int character_switch_request(enum CharacterSwitchId id) {
    character_switch_cancel_pending();
    if (!character_switch_available(id) || character_switch_can_open()) return 0;
    struct MarioState *m = &gMarioStates[0];
    sPending = id;
    sPendingFrom = sActive;
    sPendingArea = m->area;
    sPendingObject = m->marioObj;
    sPendingLevel = gCurrLevelNum;
    sPendingOnline = character_switch_online();
    sPendingAreaSequence = sPendingOnline ? gNetworkPlayerLocal->currLevelAreaSeqId : 0;
    sPendingGlobal = sPendingOnline ? gNetworkPlayerLocal->globalIndex : 0;
    return 1;
}
int character_switch_apply_pending(void) {
    if (sPending < 0) return 0;
    enum CharacterSwitchId id = (enum CharacterSwitchId)sPending;
    struct MarioState *m = &gMarioStates[0];
    int sameOwner = sActive == sPendingFrom && m->area == sPendingArea &&
        m->marioObj == sPendingObject && gCurrLevelNum == sPendingLevel;
    sameOwner = sameOwner && sPendingOnline == character_switch_online() &&
        (!sPendingOnline || (gNetworkPlayerLocal &&
         gNetworkPlayerLocal->currLevelAreaSeqId == sPendingAreaSequence &&
         gNetworkPlayerLocal->globalIndex == sPendingGlobal));
    character_switch_cancel_pending();
    if (!sameOwner) return -1;
    return character_switch_commit(id) ? 1 : -1; /* Revalidates all safety/asset gates. */
}
int character_switch_snapshot(CharacterSwitchSnapshot *out) {
    if (!out || !sInitialized) return 0;
    *out = sSnapshot;
    return 1;
}
