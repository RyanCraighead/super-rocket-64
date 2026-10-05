#pragma once
#include <PR/ultratypes.h>
#include <stdbool.h>
#include <stddef.h>

enum RomAssetType {
    ROM_ASSET_VTX,
    ROM_ASSET_TEXTURE,
    ROM_ASSET_SAMPLE,
    ROM_ASSET_COLLISION,
    ROM_ASSET_ANIM,
    ROM_ASSET_DIALOG,
    ROM_ASSET_DEMO,
    ROM_ASSET_U16,
    ROM_ASSET_LIGHT,
    ROM_ASSET_GFX_SCALAR,
    ROM_ASSET_DYNLIST,
};

// Host pointers are explicit bindings, never raw addresses copied from the ROM.
struct DynList;
struct RomDynlistBinding {
    u32 commandIndex;
    u8 word;
    u32 romValue;
    void* hostValue;
};

#define ROM_ASSET_LOAD_DYNLIST(_name, _physicalAddress, _byteCount, _bindings, _bindingCount) \
typedef char _name ## _dynlist_layout_check[(sizeof(s32) == 4 && sizeof(f32) == 4 && (_byteCount) > 0 && (_byteCount) % 24 == 0) ? 1 : -1]; \
struct DynList _name[(_byteCount) / 24] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue_dynlist(_name, _physicalAddress, _byteCount, _bindings, _bindingCount); \
}

void rom_assets_queue_dynlist(struct DynList* destination, u32 physicalAddress, u32 byteCount, const struct RomDynlistBinding* bindings, size_t bindingCount);
bool rom_assets_decode_dynlist(struct DynList* destination, size_t commandCount, const u8* segment, size_t segmentSize, size_t offset, size_t byteCount, const struct RomDynlistBinding* bindings, size_t bindingCount);

// Fixed-width structural words have no host pointers. The loader converts
// big-endian ROM words into native-endian storage after validating the full span.
#define ROM_ASSET_LOAD_U16(_type, _name, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
typedef char _name ## _word_width_check[(sizeof(_type) == 2 && (_segmentedSize) % 2 == 0) ? 1 : -1]; \
_type _name[(_segmentedSize) / 2] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_U16, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

// Preserve the signedness, row width and static linkage of pointer-free geometry.
// The existing bounded U16 decoder converts each original big-endian word.
#define ROM_ASSET_LOAD_U16_2D(_type, _name, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize, _columns) \
typedef char _name ## _word_array_check[(sizeof(_type) == 2 && (_columns) > 0 && (_segmentedSize) > 0 && (_segmentedSize) % (2 * (_columns)) == 0) ? 1 : -1]; \
static _type _name[(_segmentedSize) / (2 * (_columns))][_columns] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_U16, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

// Lights1 contains byte fields and padding only: no pointers or endian words.
// Keep an explicit ABI guard; a changed host layout must fail compilation.
#define ROM_ASSET_LOAD_LIGHT(_name, _physicalAddress, _physicalSize, _segmentedAddress) \
typedef char _name ## _light_layout_check[(sizeof(Lights1) == 24 && offsetof(Lights1, a) == 0 && offsetof(Lights1, l) == 8 && sizeof(Ambient) == 8 && sizeof(Light) == 16 && offsetof(Light_t, dir) == 8) ? 1 : -1]; \
static Lights1 _name = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(&_name, ROM_ASSET_LIGHT, _physicalAddress, _physicalSize, _segmentedAddress, 24); \
}

// Verified pointer-free F3D lists only; the decoder translates RSP commands
// through this build's GBI macros. Never copy an N64 packet into a native Gfx.
#define ROM_ASSET_LOAD_GFX(_linkage, _name, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
typedef char _name ## _gfx_layout_check[(sizeof(Gfx) == 2 * sizeof(uintptr_t) && offsetof(Gwords, w1) == sizeof(uintptr_t) && (_segmentedSize) > 0 && (_segmentedSize) % 8 == 0) ? 1 : -1]; \
_linkage Gfx _name[(_segmentedSize) / 8] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_GFX_SCALAR, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_VTX(_name, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
Vtx _name[(_segmentedSize) / 16] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_VTX, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_TEXTURE(_name, _filename, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
ALIGNED8 Texture _name[_segmentedSize] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_TEXTURE, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_SAMPLE(_name, _ptr, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_ptr, ROM_ASSET_SAMPLE, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_SEQUENCE(_name, _ptr, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_ptr, ROM_ASSET_SAMPLE, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_COLLISION(_name, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
Collision _name[(_segmentedSize) / 2] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_COLLISION, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_ANIM(_name, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
static u16 _name[(_segmentedSize) / 2] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_ANIM, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_ANIM_2D(_name, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize, _secondDim) \
static u16 _name[(_segmentedSize) / (2 * _secondDim)][_secondDim] = { 0 }; \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_name, ROM_ASSET_ANIM, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_MARIO_ANIM(_name, _ptr, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_ptr, ROM_ASSET_ANIM, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_DIALOG(_ptr, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
__attribute__((constructor)) static void _ptr ## _rom_assets_queue () { \
    rom_assets_queue(_ptr, ROM_ASSET_DIALOG, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

#define ROM_ASSET_LOAD_DEMO(_name, _ptr, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize) \
__attribute__((constructor)) static void _name ## _rom_assets_queue () { \
    rom_assets_queue(_ptr, ROM_ASSET_DEMO, _physicalAddress, _physicalSize, _segmentedAddress, _segmentedSize); \
}

void rom_assets_load(void);
void rom_assets_queue(void* ptr, enum RomAssetType assetType, u32 physicalAddress, u32 physicalSize, u32 segmentedAddress, u32 segmentedSize);
u8* rom_assets_decompress(u32* data, u32* decompressedSize);

// Failure preserves the entire destination; source/destination must not overlap.
bool rom_assets_decode_u16(void* destination, size_t wordCount, const u8* segment, size_t segmentSize, size_t offset, size_t byteCount);

// Exact 24-byte original light layout; failure preserves destination.
bool rom_assets_decode_light(void* destination, const u8* segment, size_t segmentSize, size_t offset);

// Bounded scalar-only F3D translation; failure preserves destination. No overlap.
bool rom_assets_decode_gfx(void* destination, size_t commandCount, const u8* segment, size_t segmentSize, size_t offset, size_t byteCount);
