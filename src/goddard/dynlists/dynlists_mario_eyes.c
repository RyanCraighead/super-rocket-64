#include "pc/rom_assets.h"
#include <PR/ultratypes.h>

#include "macros.h"
#include "dynlist_macros.h"
#include "dynlists.h"
#include "../dynlist_proc.h"

ROM_ASSET_LOAD_U16_2D(s16, verts_mario_eye_right, 0x00276fb0, 288, 0x00000000, 288, 3);

static struct GdVtxData vtx_mario_eye_right = { ARRAY_COUNT(verts_mario_eye_right), 0x1, verts_mario_eye_right };

ROM_ASSET_LOAD_U16_2D(u16, facedata_mario_eye_right, 0x002770dc, 656, 0x00000000, 656, 4);

static struct GdFaceData faces_mario_eye_right = { ARRAY_COUNT(facedata_mario_eye_right), 0x1, facedata_mario_eye_right };

static const struct RomDynlistBinding dynlist_mario_eye_right_shape_bindings[] = {
    { 2, 1, 0x04003730u, (void *)(&vtx_mario_eye_right) },
    { 4, 1, 0x040039ccu, (void *)(&faces_mario_eye_right) },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_mario_eye_right_shape, 0x00277378, 672, dynlist_mario_eye_right_shape_bindings, 2);

ROM_ASSET_LOAD_U16_2D(s16, verts_mario_eye_left, 0x00277618, 288, 0x00000000, 288, 3);

static struct GdVtxData vtx_mario_eye_left = { ARRAY_COUNT(verts_mario_eye_left), 0x1, verts_mario_eye_left };

ROM_ASSET_LOAD_U16_2D(u16, facedata_mario_eye_left, 0x00277744, 656, 0x00000000, 656, 4);

static struct GdFaceData faces_mario_eye_left = { ARRAY_COUNT(facedata_mario_eye_left), 0x1, facedata_mario_eye_left };

static const struct RomDynlistBinding dynlist_mario_eye_left_shape_bindings[] = {
    { 2, 1, 0x04003d98u, (void *)(&vtx_mario_eye_left) },
    { 4, 1, 0x04004034u, (void *)(&faces_mario_eye_left) },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_mario_eye_left_shape, 0x002779e0, 672, dynlist_mario_eye_left_shape_bindings, 2);
