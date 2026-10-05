/* Original standalone single-player adapter for the full SM64 engine.
 * No sockets, addresses, polling, external IDs, or packet delivery exist here.
 * NT_SERVER remains an engine-local authority marker so original save and object
 * ownership paths work. It does not imply a listening/connected transport.
 */
#include "codex_offline.h"
#include "network/network.h"
#include "configfile.h"
#include "debuglog.h"
#include "djui/djui.h"
#include "djui/djui_panel.h"
#include "utils/misc.h"
#include "game/area.h"
#include "game/hardcoded.h"
#include "game/level_update.h"
#include "game/save_file.h"

static bool offline_initialize(enum NetworkType type, bool reconnecting) {
    return gCLIOpts.offline && type == NT_NONE && !reconnecting;
}
static s64 offline_get_id(UNUSED u8 index) { return -1; }
static char* offline_get_id_str(UNUSED u8 index) { return ""; }
static void offline_save_id(UNUSED u8 index, UNUSED s64 id) {}
static void offline_clear_id(UNUSED u8 index) {}
static void* offline_dup_addr(UNUSED u8 index) { return NULL; }
static bool offline_match_addr(UNUSED void* a, UNUSED void* b) { return false; }
static void offline_update(void) {}
static int offline_send(UNUSED u8 index, UNUSED void* address, UNUSED u8* data, UNUSED u16 length) {
    return -1; /* A packet is never represented as delivered. */
}
static void offline_get_lobby(char* destination, u32 length) {
    if (destination != NULL && length > 0) { destination[0] = '\0'; }
}
static void offline_shutdown(UNUSED bool reconnecting) {}

struct NetworkSystem gCodexOfflineSystem = {
    .initialize = offline_initialize,
    .get_id = offline_get_id,
    .get_id_str = offline_get_id_str,
    .save_id = offline_save_id,
    .clear_id = offline_clear_id,
    .dup_addr = offline_dup_addr,
    .match_addr = offline_match_addr,
    .update = offline_update,
    .send = offline_send,
    .get_lobby_id = offline_get_lobby,
    .get_lobby_secret = offline_get_lobby,
    .shutdown = offline_shutdown,
    .requireServerBroadcast = false,
    .name = "Offline",
};

bool codex_offline_start(void) {
    if (!gCLIOpts.offline) { return false; }
    network_reset_reconnect_and_rehost();
    stop_demo(NULL);
    djui_panel_shutdown();
    if (configHostSaveSlot < 1 || configHostSaveSlot > NUM_SAVE_FILES) {
        configHostSaveSlot = 1;
    }
    gCurrSaveFileNum = configHostSaveSlot;
    update_all_mario_stars();
    if (!network_init(NT_NONE, false)) {
        LOG_ERROR("standalone initialization failed; no network fallback attempted");
        return false;
    }
    fake_lvl_init_from_save_file();
    extern s16 gChangeLevelTransition;
    gChangeLevelTransition = gLevelValues.entryLevel;
    play_transition(WARP_TRANSITION_FADE_INTO_STAR, 0x14, 0x00, 0x00, 0x00);
    LOG_INFO("standalone single-player started; no transport, local save slot %d", gCurrSaveFileNum);
    return true;
}
