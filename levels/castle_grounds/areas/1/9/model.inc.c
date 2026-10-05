#include "pc/rom_assets.h"
// 0x0700BCD8 - 0x0700BCE0
ROM_ASSET_LOAD_LIGHT(castle_grounds_seg7_lights_0700BCD8, 0x0044abc0, 39442, 0x0000bcd8);

// 0x0700BCF0 - 0x0700BD08
ROM_ASSET_LOAD_LIGHT(castle_grounds_seg7_lights_0700BCF0, 0x0044abc0, 39442, 0x0000bcf0);

// 0x0700BD08 - 0x0700BD20
ROM_ASSET_LOAD_LIGHT(castle_grounds_seg7_lights_0700BD08, 0x0044abc0, 39442, 0x0000bd08);

// 0x0700BD20 - 0x0700BD38
ROM_ASSET_LOAD_LIGHT(castle_grounds_seg7_lights_0700BD20, 0x0044abc0, 39442, 0x0000bd20);

// 0x0700BD38 - 0x0700BD50
ROM_ASSET_LOAD_LIGHT(castle_grounds_seg7_lights_0700BD38, 0x0044abc0, 39442, 0x0000bd38);

// 0x0700BD50 - 0x0700BE10
ROM_ASSET_LOAD_VTX(castle_grounds_seg7_vertex_0700BD50, 0x0044abc0, 39442, 0x0000bd50, 192);

// 0x0700BE10 - 0x0700BE90
ROM_ASSET_LOAD_VTX(castle_grounds_seg7_vertex_0700BE10, 0x0044abc0, 39442, 0x0000be10, 128);

// 0x0700BE90 - 0x0700BF50
ROM_ASSET_LOAD_VTX(castle_grounds_seg7_vertex_0700BE90, 0x0044abc0, 39442, 0x0000be90, 192);

// 0x0700BF50 - 0x0700BFB0
ROM_ASSET_LOAD_VTX(castle_grounds_seg7_vertex_0700BF50, 0x0044abc0, 39442, 0x0000bf50, 96);

// 0x0700BFB0 - 0x0700C010
ROM_ASSET_LOAD_VTX(castle_grounds_seg7_vertex_0700BFB0, 0x0044abc0, 39442, 0x0000bfb0, 96);

// 0x0700C010 - 0x0700C070
ROM_ASSET_LOAD_VTX(castle_grounds_seg7_vertex_0700C010, 0x0044abc0, 39442, 0x0000c010, 96);

// 0x0700C070 - 0x0700C110
static const Gfx castle_grounds_seg7_dl_0700C070[] = {
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, outside_09004800),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 32 * 64 - 1, CALC_DXT(32, G_IM_SIZ_16b_BYTES)),
    gsSPLight(&castle_grounds_seg7_lights_0700BCD8.l, 1),
    gsSPLight(&castle_grounds_seg7_lights_0700BCD8.a, 2),
    gsSPVertex(castle_grounds_seg7_vertex_0700BD50, 12, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  2,  3, 0x0),
    gsSP2Triangles( 4,  5,  6, 0x0,  4,  7,  5, 0x0),
    gsSP2Triangles( 8,  9, 10, 0x0,  8, 11,  9, 0x0),
    gsSPLight(&castle_grounds_seg7_lights_0700BCF0.l, 1),
    gsSPLight(&castle_grounds_seg7_lights_0700BCF0.a, 2),
    gsSPVertex(castle_grounds_seg7_vertex_0700BE10, 8, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  3,  1, 0x0),
    gsSP2Triangles( 4,  5,  6, 0x0,  4,  7,  5, 0x0),
    gsSPEndDisplayList(),
};

// 0x0700C110 - 0x0700C210
static const Gfx castle_grounds_seg7_dl_0700C110[] = {
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, outside_09001000),
    gsDPLoadSync(),
    gsDPLoadBlock(G_TX_LOADTILE, 0, 0, 64 * 32 - 1, CALC_DXT(64, G_IM_SIZ_16b_BYTES)),
    gsSPLight(&castle_grounds_seg7_lights_0700BD08.l, 1),
    gsSPLight(&castle_grounds_seg7_lights_0700BD08.a, 2),
    gsSPVertex(castle_grounds_seg7_vertex_0700BE90, 12, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  0,  3,  1, 0x0),
    gsSP2Triangles( 0,  4,  3, 0x0,  0,  2,  5, 0x0),
    gsSP2Triangles( 6,  4,  7, 0x0,  6,  3,  4, 0x0),
    gsSP2Triangles( 2,  6,  7, 0x0,  5,  2,  7, 0x0),
    gsSP2Triangles( 8,  9, 10, 0x0,  9, 11, 10, 0x0),
    gsSPLight(&castle_grounds_seg7_lights_0700BD20.l, 1),
    gsSPLight(&castle_grounds_seg7_lights_0700BD20.a, 2),
    gsSPVertex(castle_grounds_seg7_vertex_0700BF50, 6, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  3,  4,  5, 0x0),
    gsSPLight(&castle_grounds_seg7_lights_0700BD38.l, 1),
    gsSPLight(&castle_grounds_seg7_lights_0700BD38.a, 2),
    gsSPVertex(castle_grounds_seg7_vertex_0700BFB0, 6, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  3,  4,  5, 0x0),
    gsSPLight(&castle_grounds_seg7_lights_0700BCF0.l, 1),
    gsSPLight(&castle_grounds_seg7_lights_0700BCF0.a, 2),
    gsSPVertex(castle_grounds_seg7_vertex_0700C010, 6, 0),
    gsSP2Triangles( 0,  1,  2, 0x0,  3,  4,  5, 0x0),
    gsSPEndDisplayList(),
};

// 0x0700C210 - 0x0700C2A0
const Gfx castle_grounds_seg7_dl_0700C210[] = {
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_MODULATERGB, G_CC_MODULATERGB),
    gsSPClearGeometryMode(G_SHADING_SMOOTH),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, G_TX_NOMASK, G_TX_NOLOD),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 0, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 6, G_TX_NOLOD, G_TX_CLAMP, 5, G_TX_NOLOD),
    gsDPSetTileSize(0, 0, 0, (32 - 1) << G_TEXTURE_IMAGE_FRAC, (64 - 1) << G_TEXTURE_IMAGE_FRAC),
    gsSPDisplayList(castle_grounds_seg7_dl_0700C070),
    gsDPTileSync(),
    gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 0, G_TX_RENDERTILE, 0, G_TX_WRAP | G_TX_NOMIRROR, 5, G_TX_NOLOD, G_TX_WRAP | G_TX_NOMIRROR, 6, G_TX_NOLOD),
    gsDPSetTileSize(0, 0, 0, (64 - 1) << G_TEXTURE_IMAGE_FRAC, (32 - 1) << G_TEXTURE_IMAGE_FRAC),
    gsSPDisplayList(castle_grounds_seg7_dl_0700C110),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF),
    gsDPPipeSync(),
    gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
    gsSPSetGeometryMode(G_SHADING_SMOOTH),
    gsSPEndDisplayList(),
};
