#include "pc/rom_assets.h"
#include <PR/ultratypes.h>

#include "macros.h"
#include "dynlist_macros.h"
#include "dynlists.h"
#include "../dynlist_proc.h"

ROM_ASSET_LOAD_U16_2D(s16, verts_mario_eyebrow_right, 0x00277c80, 156, 0x00000000, 156, 3);

static struct GdVtxData vtx_mario_eyebrow_right = { ARRAY_COUNT(verts_mario_eyebrow_right), 0x1, verts_mario_eyebrow_right };

ROM_ASSET_LOAD_U16_2D(u16, facedata_mario_eyebrow_right, 0x00277d28, 288, 0x00000000, 288, 4);

static struct GdFaceData faces_mario_eyebrow_right = { ARRAY_COUNT(facedata_mario_eyebrow_right), 0x1, facedata_mario_eyebrow_right };

static const struct RomDynlistBinding dynlist_mario_eyebrow_right_shape_bindings[] = {
    { 2, 1, 0x0400437cu, (void *)(&vtx_mario_eyebrow_right) },
    { 4, 1, 0x040044a8u, (void *)(&faces_mario_eyebrow_right) },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_mario_eyebrow_right_shape, 0x00277e54, 384, dynlist_mario_eyebrow_right_shape_bindings, 2);

ROM_ASSET_LOAD_U16_2D(s16, verts_mario_eyebrow_left, 0x00277fd4, 156, 0x00000000, 156, 3);

static struct GdVtxData vtx_mario_eyebrow_left = { ARRAY_COUNT(verts_mario_eyebrow_left), 0x1, verts_mario_eyebrow_left };

ROM_ASSET_LOAD_U16_2D(u16, facedata_mario_eyebrow_left, 0x0027807c, 288, 0x00000000, 288, 4);

static struct GdFaceData faces_mario_eyebrow_left = { ARRAY_COUNT(facedata_mario_eyebrow_left), 0x1, facedata_mario_eyebrow_left };

static const struct RomDynlistBinding dynlist_mario_eyebrow_left_shape_bindings[] = {
    { 2, 1, 0x040046d0u, (void *)(&vtx_mario_eyebrow_left) },
    { 4, 1, 0x040047fcu, (void *)(&faces_mario_eyebrow_left) },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_mario_eyebrow_left_shape, 0x002781a8, 384, dynlist_mario_eyebrow_left_shape_bindings, 2);

ROM_ASSET_LOAD_U16_2D(s16, verts_mario_mustache, 0x00278328, 336, 0x00000000, 336, 3);

static struct GdVtxData vtx_mario_mustache = { ARRAY_COUNT(verts_mario_mustache), 0x1, verts_mario_mustache };

ROM_ASSET_LOAD_U16_2D(u16, facedata_mario_mustache, 0x00278484, 800, 0x00000000, 800, 4);

static struct GdFaceData faces_mario_mustache = { ARRAY_COUNT(facedata_mario_mustache), 0x1, facedata_mario_mustache };

static const struct RomDynlistBinding dynlist_mario_mustache_shape_bindings[] = {
    { 2, 1, 0x04004ad8u, (void *)(&vtx_mario_mustache) },
    { 4, 1, 0x04004e04u, (void *)(&faces_mario_mustache) },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_mario_mustache_shape, 0x002787b0, 384, dynlist_mario_mustache_shape_bindings, 2);
