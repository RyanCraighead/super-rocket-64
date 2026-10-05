#include "pc/local_engine_assets.h"
#include "pc/rom_assets.h"
// 0x07000000 - 0x07001000
ROM_ASSET_LOAD_TEXTURE(castle_grounds_seg7_texture_07000000, "levels/castle_grounds/0.rgba16.inc.c", 0x0044abc0, 39442, 0x00000000, 4096);

// 0x07001000 - 0x07002000
ROM_ASSET_LOAD_TEXTURE(castle_grounds_seg7_texture_07001000, "levels/castle_grounds/1.rgba16.inc.c", 0x0044abc0, 39442, 0x00001000, 4096);

// 0x07002000 - 0x07003000
ROM_ASSET_LOAD_TEXTURE(castle_grounds_seg7_texture_07002000, "levels/castle_grounds/2.rgba16.inc.c", 0x0044abc0, 39442, 0x00002000, 4096);

ALIGNED8 Texture castle_grounds_seg7_texture_07003000[4096] = {0};
LOCAL_ENGINE_ASSET_REGISTER(castle_grounds_seg7_texture_07003000, "textures/levels/castle_grounds/6_custom.rgba16.bin", "2f115c8ca71e9900b7514c4998e065f1fed0dc8391a7be63bb62895efc10f223");
