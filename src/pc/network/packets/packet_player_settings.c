#include <stdio.h>
#include "../network.h"
#include "game/character_switch.h"
#include "pc/debuglog.h"

void network_send_player_settings(void) {
    char playerName[MAX_CONFIG_STRING] = { 0 };
    if (snprintf(playerName, MAX_CONFIG_STRING, "%s", configPlayerName) < 0) {
        LOG_INFO("truncating player name");
    }

    struct Packet p = { 0 };
    packet_init(&p, PACKET_PLAYER_SETTINGS, true, PLMT_NONE);
    packet_write(&p, &gNetworkPlayers[0].globalIndex, sizeof(u8));
    packet_write(&p, playerName, MAX_CONFIG_STRING * sizeof(u8));
    u8 effectiveModel = character_switch_native_model(0, configPlayerModel);
    packet_write(&p, &effectiveModel, sizeof(u8));
    packet_write(&p, &configPlayerPalette, sizeof(struct PlayerPalette));

    if (gNetworkPlayerLocal != NULL) {
        /* The native settings UI first copies the saved preference into this
         * slot. Keep later peer-list broadcasts consistent with this packet,
         * without changing that preference or an independent model override. */
        if (gNetworkPlayerLocal->overrideModelIndex == gNetworkPlayerLocal->modelIndex) {
            gNetworkPlayerLocal->overrideModelIndex = effectiveModel;
        }
        gNetworkPlayerLocal->modelIndex = effectiveModel;
        if (snprintf(gNetworkPlayerLocal->name, MAX_CONFIG_STRING, "%s", playerName) < 0) {
            LOG_INFO("truncating player name");
        }
    }

    network_send(&p);
}

void network_receive_player_settings(struct Packet* p) {
    u8 globalId;
    char playerName[MAX_CONFIG_STRING] = { 0 };
    u8 playerModel;
    struct PlayerPalette playerPalette;

    packet_read(p, &globalId, sizeof(u8));
    packet_read(p, &playerName, MAX_CONFIG_STRING * sizeof(u8));
    packet_read(p, &playerModel, sizeof(u8));
    packet_read(p, &playerPalette, sizeof(struct PlayerPalette));

    if (globalId == gNetworkPlayers[0].globalIndex || globalId > MAX_PLAYERS) {
        LOG_ERROR("Received player settings from improper player.");
        return;
    }

    // anti spoof
    if (packet_spoofed(p, globalId)) {
        LOG_ERROR("rx spoofed player settings");
        return;
    }

    // sanity check
    if (playerModel >= CT_MAX) { playerModel = CT_MARIO; }

    struct NetworkPlayer* np = network_player_from_global_index(globalId);
    if (!np) { LOG_ERROR("Failed to retrieve network player."); return; }
    if (snprintf(np->name, MAX_CONFIG_STRING, "%s", playerName) < 0) {
        LOG_INFO("truncating player name");
    }

    if (np->modelIndex   == np->overrideModelIndex)   { np->overrideModelIndex   = playerModel;   }
    if (memcmp(&np->palette, &np->overridePalette, sizeof(struct PlayerPalette)) == 0) { np->overridePalette = playerPalette; }

    np->modelIndex   = playerModel;
    np->palette      = playerPalette;

    network_player_update_model(np->localIndex);
}
