#include "pc/rom_assets.h"
#include <PR/ultratypes.h>

#include "macros.h"
#include "dynlist_macros.h"
#include "dynlists.h"
#include "../dynlist_proc.h"

ROM_ASSET_LOAD_U16_2D(s16, mario_Face_VtxData, 0x002745c0, 2640, 0x00000000, 2640, 3);

static struct GdVtxData mario_Face_VtxInfo = { ARRAY_COUNT(mario_Face_VtxData), 0x1, mario_Face_VtxData };

ROM_ASSET_LOAD_U16_2D(u16, mario_Face_FaceData, 0x0027501c, 7016, 0x00000000, 7016, 4);

static struct GdFaceData mario_Face_FaceInfo = { ARRAY_COUNT(mario_Face_FaceData), 0x1, mario_Face_FaceData };

static const struct RomDynlistBinding dynlist_mario_face_shape_bindings[] = {
    { 2, 1, 0x04001670u, (void *)(&mario_Face_VtxInfo) },
    { 4, 1, 0x040031e4u, (void *)(&mario_Face_FaceInfo) },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_mario_face_shape, 0x00276b90, 1056, dynlist_mario_face_shape_bindings, 2);
