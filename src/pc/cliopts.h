#ifndef _CLIOPTS_H
#define _CLIOPTS_H

#include "platform.h"
#include "pc/configfile.h"

enum NetworkType {
    NT_NONE,
    NT_SERVER,
    NT_CLIENT
};

#define IP_MAX_LEN 32
#define PORT_MAX_LEN 16

struct CLIOptions {
#if defined(_WIN32)
    bool console;
#endif
    char savePath[SYS_MAX_PATH];
    char configFile[SYS_MAX_PATH];
    unsigned int fullscreen;
    unsigned int width;
    unsigned int height;
    bool skipIntro;
    enum NetworkType network;
    unsigned int networkPort;
    char joinIp[IP_MAX_LEN];
    char playerName[MAX_CONFIG_STRING];
    unsigned int playerCount;
    bool hideLoadingScreen;
    bool skipUpdateCheck;
    bool noDiscord;
    bool coopnet;
    char coopnetPassword[MAX_CONFIG_STRING];
    bool disableMods;
    int enabledModsCount;
    char** enableMods;
    bool headless;
    bool offline;
    bool characterWheel;
    bool ootLink;
    bool rocketCar;
    bool characterNet;
    bool loopbackOnly;
    char characterNetAssets[SYS_MAX_PATH];
    char rocketAssets[SYS_MAX_PATH];
    char ootLinkAssets[SYS_MAX_PATH];
    bool bkDuo;
    char bkAssets[SYS_MAX_PATH];
    bool spidermanOriginal;
    char spidermanAssets[SYS_MAX_PATH];
    bool thpsOriginal;
    char thpsAssets[SYS_MAX_PATH];
    bool bm64Bomberman;
    char bm64Assets[SYS_MAX_PATH];
#if defined(_WIN32)
    int backend;
#endif
};

extern struct CLIOptions gCLIOpts;

bool parse_cli_opts(int argc, char* argv[]);

#endif // _CLIOPTS_H
