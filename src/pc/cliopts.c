#include "cliopts.h"
#include "configfile.h"
#include "pc_main.h"
#include "platform.h"
#include "macros.h"

#include <strings.h>
#include <stdlib.h>
#define __NO_MINGW_LFS //Mysterious error in MinGW.org stdio.h
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

struct CLIOptions gCLIOpts;

static void print_help(void) {
    printf("sm64coopdx\n");
#if defined(_WIN32)
    printf("--console                 Enables the Windows console.\n");
#endif
    printf("--savepath SAVEPATH       Overrides the default save/config path ('!' expands to executable path).\n");
    printf("--configfile CONFIGNAME   Saves the configuration file as CONFIGNAME.\n");
    printf("--hide-loading-screen     Hides the loading screen before the menu boots up.\n");
    printf("--fullscreen              Starts the game in full screen mode.\n");
    printf("--windowed                Starts the game in windowed mode.\n");
    printf("--width WIDTH             Sets the window width.\n");
    printf("--height HEIGHT           Sets the window height.\n");
    printf("--skip-intro              Skips the Peach and Lakitu intros when on a zero star save.\n");
    printf("--server PORT             Starts the game and creates a new server on PORT.\n");
    printf("--client IP PORT          Starts the game and joins an existing server.\n");
    printf("--coopnet PASSWORD        Starts the game and creates a new CoopNet server.\n");
    printf("--playername PLAYERNAME   Starts the game with a specific playername.\n");
    printf("--playercount PLAYERCOUNT Starts the game with a specific player count limit.\n");
    printf("--skip-update-check       Skips the update check when loading the game.\n");
    printf("--no-discord              Disables discord integration.\n");
    printf("--disable-mods            Disables all mods that are already enabled.\n");
    printf("--enable-mod MODNAME      Enables a mod.\n");
    printf("--headless                Enable Headless mode.\n");
    printf("--offline                 Play local single-player without networking.\n");
    printf("--character-wheel         Hold F7 to switch characters (offline, or Mario/Octane with --character-net; mods disabled).\n");
    printf("--oot-link DIRECTORY       Use privately extracted original OoT Link (experimental, offline only).\n");
    printf("--rocket-car DIRECTORY     Original Octane geometry with approximate RocketSim physics.\n");
    printf("--character-net DIRECTORY  Opt-in Mario/Octane network protocol; private Octane assets on every peer.\n");
    printf("--loopback-only            Bind direct-IP sockets to ::1 for local tests.\n");
    printf("--bk-duo DIRECTORY         Use privately extracted original Banjo-Kazooie (experimental, offline only).\n");
    printf("--spiderman-original DIR  Use original N64 Spider-Man (bounded movement slice, offline only).\n");
    printf("--thps-original DIR       Use original THPS1 N64 skater assets (experimental skating slice, offline only).\n");
    printf("--bm64-bomberman DIRECTORY Use privately extracted original Bomberman64 (experimental, offline only).\n");
#if defined(_WIN32)
    printf("--backend                 Sets the backend to either 'opengl' or 'directx'.");
#endif
}

static inline int arg_string(const char *name, const char *value, char *target, int maxLength) {
    if (!value || !*value || !strncmp(value, "--", 2)) { fprintf(stderr, "Missing value for `%s`.\n", name); return 0; }
    const unsigned int arglen = strlen(value);
    if (arglen >= (unsigned int) maxLength) {
        fprintf(stderr, "Supplied value for `%s` is too long.\n", name);
        return 0;
    }
    snprintf(target, maxLength, "%s", value);
    return 1;
}

static inline int arg_uint(const char *name, const char *value, unsigned int *target) {
    char *end = NULL;
    errno = 0;
    if (!value || !*value || value[0] == '-' || value[0] == '+') goto invalid;
    unsigned long v = strtoul(value, &end, 0);
    if (errno || end == value || *end || v > UINT_MAX) goto invalid;
    *target = (unsigned int)v;
    return 1;
invalid:
    fprintf(stderr, "Invalid or missing unsigned value for `%s`.\n", name);
    return 0;
}

bool parse_cli_opts(int argc, char* argv[]) {
    // initialize options with false values
    memset(&gCLIOpts, 0, sizeof(gCLIOpts));
    gCLIOpts.enableMods = NULL;
    /* Honor the explicit offline intent even if another malformed option would
       otherwise consume the flag as its value. Reject online combinations early. */
    bool requestedOffline = false, requestedOnline = false;
    for (int j = 1; j < argc; j++) {
        if (!strcmp(argv[j], "--offline")) requestedOffline = true;
        if (!strcmp(argv[j], "--client") || !strcmp(argv[j], "--server") || !strcmp(argv[j], "--coopnet")) requestedOnline = true;
    }
    if (requestedOffline && requestedOnline) { fprintf(stderr, "--offline conflicts with network options.\n"); return false; }
    if (requestedOffline) { gCLIOpts.offline = true; gCLIOpts.skipUpdateCheck = true; gCLIOpts.noDiscord = true; }
#if defined(_WIN32)
    gCLIOpts.backend = -1;
#endif

    for (int i = 1; i < argc; i++) {
#if defined(_WIN32)
        if (!strcmp(argv[i], "--console")) {
            gCLIOpts.console = true;
        } else if (!strcmp(argv[i], "--savepath") && (i + 1) < argc) {
#else
        if (!strcmp(argv[i], "--savepath") && (i + 1) < argc) {
#endif
            if (!arg_string("--savepath", argv[++i], gCLIOpts.savePath, SYS_MAX_PATH)) return false;
        } else if (!strcmp(argv[i], "--configfile") && (i + 1) < argc) {
            if (!arg_string("--configfile", argv[++i], gCLIOpts.configFile, SYS_MAX_PATH)) return false;
        } else if (!strcmp(argv[i], "--hide-loading-screen")) {
            gCLIOpts.hideLoadingScreen = true;
        } else if (!strcmp(argv[i], "--fullscreen")) {
            gCLIOpts.fullscreen = 1;
        } else if (!strcmp(argv[i], "--windowed")) {
            gCLIOpts.fullscreen = 2;
        } else if (!strcmp(argv[i], "--width")) {
            if (i + 1 >= argc || !arg_uint("--width", argv[++i], &gCLIOpts.width)) return false;
        } else if (!strcmp(argv[i], "--height")) {
            if (i + 1 >= argc || !arg_uint("--height", argv[++i], &gCLIOpts.height)) return false;
        } else if (!strcmp(argv[i], "--skip-intro")) {
            gCLIOpts.skipIntro = true;
        } else if (!strcmp(argv[i], "--server") && (i + 1) < argc) {
            gCLIOpts.network = NT_SERVER;
            if (!arg_uint("--server", argv[++i], &gCLIOpts.networkPort)) return false;
        } else if (!strcmp(argv[i], "--client") && (i + 1) < argc) {
            gCLIOpts.network = NT_CLIENT;
            if (!arg_string("--client <ip>", argv[++i], gCLIOpts.joinIp, IP_MAX_LEN)) return false;
            if ((i + 1) < argc && argv[i + 1][0] != '-') {
                if (!arg_uint("--client port", argv[++i], &gCLIOpts.networkPort)) return false;
            } else {
                gCLIOpts.networkPort = 7777;
            }
        } else if (!strcmp(argv[i], "--coopnet") && (i + 1) < argc && argv[i + 1][0] != '-') {
            gCLIOpts.coopnet = true;
            if (!arg_string("--coopnet <password>", argv[++i], gCLIOpts.coopnetPassword, MAX_CONFIG_STRING)) return false;
        } else if (!strcmp(argv[i], "--playername") && (i + 1) < argc) {
            if (!arg_string("--playername <playername>", argv[++i], gCLIOpts.playerName, MAX_CONFIG_STRING)) return false;
        } else if (!strcmp(argv[i], "--playercount") && (i + 1) < argc) {
            if (!arg_uint("--playercount", argv[++i], &gCLIOpts.playerCount)) return false;
        } else if (!strcmp(argv[i], "--skip-update-check")) {
            gCLIOpts.skipUpdateCheck = true;
        } else if (!strcmp(argv[i], "--no-discord")) {
            gCLIOpts.noDiscord = true;
        } else if (!strcmp(argv[i], "--disable-mods")) {
            gCLIOpts.disableMods = true;
        } else if (!strcmp(argv[i], "--enable-mod") && (i + 1) < argc) {
            gCLIOpts.enabledModsCount++;
            if (gCLIOpts.enableMods == NULL) {
                gCLIOpts.enableMods = malloc(sizeof(char*));
            } else {
                gCLIOpts.enableMods = realloc(gCLIOpts.enableMods, sizeof(char*) * gCLIOpts.enabledModsCount);
            }
            gCLIOpts.enableMods[gCLIOpts.enabledModsCount - 1] = strdup(argv[++i]);
        } else if (!strcmp(argv[i], "--headless")) {
            gCLIOpts.headless = true;
        } else if (!strcmp(argv[i], "--offline")) {
            gCLIOpts.offline = true;
            gCLIOpts.skipUpdateCheck = true;
            gCLIOpts.noDiscord = true;
        } else if (!strcmp(argv[i], "--character-wheel")) {
            gCLIOpts.characterWheel = true;
        } else if (!strcmp(argv[i], "--oot-link")) {
            if (i + 1 >= argc) { fprintf(stderr, "--oot-link requires its extracted asset directory.\n"); return false; }
            if (!arg_string("--oot-link", argv[++i], gCLIOpts.ootLinkAssets, SYS_MAX_PATH)) return false;
            gCLIOpts.ootLink = true;
        } else if (!strcmp(argv[i], "--bk-duo")) {
            if (i + 1 >= argc) { fprintf(stderr, "--bk-duo requires original assets.\n"); return false; }
            if (!arg_string("--bk-duo", argv[++i], gCLIOpts.bkAssets, SYS_MAX_PATH)) return false;
            gCLIOpts.bkDuo = true;
        } else if (!strcmp(argv[i], "--character-net")) {
            if (i + 1 >= argc || !arg_string("--character-net", argv[++i], gCLIOpts.characterNetAssets, SYS_MAX_PATH)) return false;
            gCLIOpts.characterNet = true;
        } else if (!strcmp(argv[i], "--loopback-only")) {
            gCLIOpts.loopbackOnly = true;
        } else if (!strcmp(argv[i], "--rocket-car")) {
            if (i + 1 >= argc) { fprintf(stderr,"--rocket-car requires private Octane assets.\n"); return false; }
            if (!arg_string("--rocket-car", argv[++i], gCLIOpts.rocketAssets, SYS_MAX_PATH)) return false;
            gCLIOpts.rocketCar = true;
        } else if (!strcmp(argv[i], "--spiderman-original")) {
            if (i + 1 >= argc) { fprintf(stderr, "--spiderman-original requires original assets.\n"); return false; }
            if (!arg_string("--spiderman-original", argv[++i], gCLIOpts.spidermanAssets, SYS_MAX_PATH)) return false;
            gCLIOpts.spidermanOriginal = true;
        } else if (!strcmp(argv[i], "--thps-original")) {
            if (i + 1 >= argc) { fprintf(stderr, "--thps-original requires original THPS1 assets.\n"); return false; }
            if (!arg_string("--thps-original", argv[++i], gCLIOpts.thpsAssets, SYS_MAX_PATH)) return false;
            gCLIOpts.thpsOriginal = true;
        } else if (!strcmp(argv[i], "--bm64-bomberman")) {
            if (i + 1 >= argc) { fprintf(stderr, "--bm64-bomberman requires original assets.\n"); return false; }
            if (!arg_string("--bm64-bomberman", argv[++i], gCLIOpts.bm64Assets, SYS_MAX_PATH)) return false;
            gCLIOpts.bm64Bomberman = true;
#if defined(_WIN32)
        } else if (!strcmp(argv[i], "--backend") && (i + 1) < argc) {
            if (!strcmp(argv[i + 1], "opengl")) {
                gCLIOpts.backend = GAPI_GL;
            } else if (!strcmp(argv[i + 1], "directx")) {
                gCLIOpts.backend = GAPI_D3D11;
            }
#endif
        } else if (!strcmp(argv[i], "--help")) {
            print_help();
            return false;
        }
    }

    if (gCLIOpts.offline && (gCLIOpts.network != NT_NONE || gCLIOpts.coopnet)) {
        fprintf(stderr, "--offline cannot be combined with network host/client options.\n");
        return false;
    }
    if (gCLIOpts.characterNet && (gCLIOpts.thpsOriginal || gCLIOpts.offline || !gCLIOpts.disableMods || gCLIOpts.enabledModsCount ||
        gCLIOpts.ootLink || gCLIOpts.bkDuo || gCLIOpts.bm64Bomberman || gCLIOpts.spidermanOriginal || gCLIOpts.coopnet)) {
        fprintf(stderr,"Character networking currently supports Mario/Octane with direct IP, --disable-mods and no other addons.\n"); return false;
    }
    if (gCLIOpts.loopbackOnly && (gCLIOpts.offline || gCLIOpts.coopnet ||
        (gCLIOpts.network == NT_CLIENT && strcmp(gCLIOpts.joinIp,"::1")))) {
        fprintf(stderr,"--loopback-only requires direct IP; clients must use ::1.\n"); return false;
    }

    if (gCLIOpts.characterWheel && ((!gCLIOpts.offline && !(gCLIOpts.characterNet &&
        (gCLIOpts.network == NT_SERVER || gCLIOpts.network == NT_CLIENT))) ||
        !gCLIOpts.disableMods || gCLIOpts.enabledModsCount || gCLIOpts.headless)) {
        fprintf(stderr, "The character wheel requires offline play or Mario/Octane direct-IP host/join, --disable-mods, no enabled mods, and a visible game window.\n");
        return false;
    }
    if (gCLIOpts.ootLink && (!gCLIOpts.offline || !gCLIOpts.disableMods || gCLIOpts.enabledModsCount)) {
        fprintf(stderr, "Original OoT Link prototype requires --offline --disable-mods and no --enable-mod.\n");
        return false;
    }
    if (gCLIOpts.bkDuo && (!gCLIOpts.offline || !gCLIOpts.disableMods || gCLIOpts.enabledModsCount || (!gCLIOpts.characterWheel && (gCLIOpts.ootLink || gCLIOpts.bm64Bomberman)))) {
        fprintf(stderr, "Original Banjo-Kazooie requires --offline --disable-mods, no --enable-mod, and no other character.\n");
        return false;
    }
    if (gCLIOpts.bm64Bomberman && (!gCLIOpts.offline || !gCLIOpts.disableMods || gCLIOpts.enabledModsCount || (!gCLIOpts.characterWheel && gCLIOpts.ootLink))) {
        fprintf(stderr, "Original Bomberman64 requires --offline --disable-mods, no --enable-mod, and no --oot-link.\n");
        return false;
    }
    if (gCLIOpts.spidermanOriginal && (!gCLIOpts.offline || !gCLIOpts.disableMods || gCLIOpts.enabledModsCount || (!gCLIOpts.characterWheel && (gCLIOpts.ootLink || gCLIOpts.bm64Bomberman || gCLIOpts.bkDuo)))) {
        fprintf(stderr, "Original Spider-Man requires --offline --disable-mods, no --enable-mod, and no other character.\n");
        return false;
    }
    if (gCLIOpts.rocketCar && ((!gCLIOpts.offline && !gCLIOpts.characterNet) || !gCLIOpts.disableMods || gCLIOpts.enabledModsCount ||
        (!gCLIOpts.characterWheel && (gCLIOpts.ootLink || gCLIOpts.bm64Bomberman || gCLIOpts.bkDuo || gCLIOpts.spidermanOriginal || gCLIOpts.thpsOriginal)))) {
        fprintf(stderr,"Rocket car requires --offline or --character-net, --disable-mods and exclusive character ownership outside the offline wheel.\n");return false;
    }
    if (gCLIOpts.thpsOriginal && (!gCLIOpts.offline || !gCLIOpts.disableMods || gCLIOpts.enabledModsCount || (!gCLIOpts.characterWheel &&
        (gCLIOpts.ootLink || gCLIOpts.bm64Bomberman || gCLIOpts.bkDuo || gCLIOpts.spidermanOriginal || gCLIOpts.rocketCar)))) {
        fprintf(stderr,"Original THPS1 requires --offline --disable-mods and exclusive ownership outside the offline wheel.\n");return false;
    }
#if defined(_WIN32)
    if ((gCLIOpts.characterWheel || gCLIOpts.bm64Bomberman || gCLIOpts.bkDuo || gCLIOpts.spidermanOriginal || gCLIOpts.thpsOriginal || gCLIOpts.rocketCar || gCLIOpts.characterNet) && gCLIOpts.backend == GAPI_D3D11) {
        fprintf(stderr,"Original character renderers and character networking require the OpenGL backend.\n");return false;
    }
    if (gCLIOpts.characterWheel || gCLIOpts.bm64Bomberman || gCLIOpts.bkDuo || gCLIOpts.spidermanOriginal || gCLIOpts.thpsOriginal || gCLIOpts.rocketCar || gCLIOpts.characterNet) gCLIOpts.backend = GAPI_GL;
#endif
    return true;
}
