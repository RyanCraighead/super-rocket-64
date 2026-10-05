#include <stdio.h>
#include "version.h"
#include "pc/cliopts.h"
#include "pc/character_net_codec.h"
#include "types.h"

static char sVersionString[MAX_VERSION_LENGTH] = { 0 };

const char* get_version(void) {
    snprintf(
        sVersionString, MAX_VERSION_LENGTH,
#if defined(VERSION_US)
        "%s%s", SM64COOPDX_VERSION, gCLIOpts.characterNet ? CNET_VERSION_SUFFIX : ""
#else
        "%s %s%s", SM64COOPDX_VERSION, VERSION_REGION, gCLIOpts.characterNet ? CNET_VERSION_SUFFIX : ""
#endif // VERSION_US
    );
    return sVersionString;
}

#ifdef COMPILE_TIME
const char* get_version_with_build_date(void) {
    snprintf(
        sVersionString, MAX_VERSION_LENGTH,
#if defined(VERSION_US)
        "%s, %s", SM64COOPDX_VERSION, COMPILE_TIME
#else
        "%s %s, %s", SM64COOPDX_VERSION, VERSION_REGION, COMPILE_TIME
#endif // VERSION_US
    );
    return sVersionString;
}
#endif