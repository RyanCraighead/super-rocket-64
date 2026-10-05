#include "pc/rom_assets.h"
// early unused test dynlist
#include <PR/ultratypes.h>

#include "dynlist_macros.h"
#include "dynlists.h"
#include "../dynlist_proc.h"
// maybe move types into the dynlists.h file?

static const struct RomDynlistBinding dynlist_test_cube_bindings[] = {
    { 1, 1, 0x040326e0u, (void *)("ico1vg") },
    { 8, 1, 0x040326e8u, (void *)("ico1vg") },
    { 9, 1, 0x040326f0u, (void *)("ico1pg") },
    { 60, 1, 0x040326f8u, (void *)("ico1pg") },
    { 61, 1, 0x04032700u, (void *)("ico1pg") },
    { 62, 1, 0x04032708u, (void *)("ico1vg") },
    { 63, 1, 0x04032710u, (void *)("ico1_sh") },
    { 64, 1, 0x04032718u, (void *)("ico1vg") },
    { 65, 1, 0x04032720u, (void *)("ico1pg") },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_test_cube, 0x002739a0, 1608, dynlist_test_cube_bindings, 9);
