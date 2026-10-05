#include "pc/rom_assets.h"
#include <PR/ultratypes.h>

#include "dynlist_macros.h"
#include "dynlists.h"
#include "../dynlist_proc.h"

static const struct RomDynlistBinding dynlist_spot_shape_bindings[] = {
    { 1, 1, 0x04032730u, (void *)("spotvg") },
    { 12, 1, 0x04032738u, (void *)("spotvg") },
    { 13, 1, 0x04032740u, (void *)("spotpg") },
    { 54, 1, 0x04032748u, (void *)("spotpg") },
    { 55, 1, 0x04032750u, (void *)("spotpg") },
    { 56, 1, 0x04032758u, (void *)("spotvg") },
    { 57, 1, 0x04032760u, (void *)("spot_sh") },
    { 58, 1, 0x04032768u, (void *)("spotvg") },
    { 59, 1, 0x04032770u, (void *)("spotpg") },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_spot_shape, 0x00273ff0, 1488, dynlist_spot_shape_bindings, 9);
