#include <PR/ultratypes.h>
#include "rom_assets.h"
#include "goddard/gd_types.h"
#include "pc/debuglog.h"
#include "rom_checker.h"
#include "apparition.inc.c"
#include "utils/misc.h"

#define ROM_ASSET_LOAD_DATA(bits) for (u##bits *data = asset->ptr; asset->cursor < asset->segmentedSize; data++) { *data = READ##bits(asset); }

struct RomAsset {
    void* ptr;
    enum RomAssetType assetType;
    u32 physicalAddress;
    u32 physicalSize;
    u32 segmentedAddress;
    u32 segmentedSize;
    u32 cursor;
    const struct RomDynlistBinding* dynBindings;
    size_t dynBindingCount;
    struct RomAsset* next;
};

static FILE* sRomFile = NULL;
static struct RomAsset* sRomAssets = NULL;

static u32 sCurrentPhysicalAddress = 0;
static u32 sCurrentPhysicalSize = 0;
static u8* sCurrentSegmentMemory = NULL;
static u32 sCurrentSegmentSize = 0;

static s16 READ16(struct RomAsset* asset) {
    s64 index = (asset->segmentedAddress + asset->cursor);
    if (index < 0 || index >= sCurrentSegmentSize) { return 0; }
    u8* ptr = &sCurrentSegmentMemory[index];
    s16 value = BSWAP16(*((s16*)ptr));
    asset->cursor += sizeof(s16);
    return value;
}

static s8 READ8(struct RomAsset* asset) {
    s64 index = (asset->segmentedAddress + asset->cursor);
    if (index < 0 || index >= sCurrentSegmentSize) { return 0; }
    u8* ptr = &sCurrentSegmentMemory[index];
    s8 value = *ptr;
    asset->cursor += sizeof(s8);
    return value;
}

static bool rom_asset_load_segment(u32 physicalAddress, u32 physicalSize) {
    if (physicalAddress == sCurrentPhysicalAddress && physicalSize == sCurrentPhysicalSize) {
        return true;
    }

    sCurrentPhysicalAddress = physicalAddress;
    sCurrentPhysicalSize = physicalSize;

    if (sCurrentSegmentMemory) {
        free(sCurrentSegmentMemory);
    }

    sCurrentSegmentMemory = malloc(physicalSize);
    sCurrentSegmentSize = physicalSize;
    if (!sCurrentSegmentMemory) {
        LOG_ERROR("Could not allocate segment memory!");
        sCurrentSegmentMemory = NULL;
        return false;
    }

    fseek(sRomFile, physicalAddress, SEEK_SET);
    fread(sCurrentSegmentMemory, sizeof(u8), physicalSize, sRomFile);

    u8* decompressed = rom_assets_decompress((u32*)sCurrentSegmentMemory, &sCurrentSegmentSize);
    if (decompressed != NULL) {
        free(sCurrentSegmentMemory);
        sCurrentSegmentMemory = decompressed;
        if (!sCurrentSegmentMemory) {
            LOG_ERROR("Could not decompress segment memory!");
        }
    }
    return (sCurrentSegmentMemory != NULL);
}

// Some Vtx arrays have been manually modified to use white opaque vertex colors
// so they can be shaded by Lua and not stand out as being unlit
static inline bool rom_asset_override_vertex_colors(void* ptr) {
    extern Vtx mario_right_hand_cap_wings_half_1_dl_vertex[];
    extern Vtx mario_right_hand_cap_wings_half_2_dl_vertex[];
    extern Vtx mario_wings_half_1_dl_vertex[];
    extern Vtx mario_wings_half_2_dl_vertex[];
    extern Vtx hoot_seg5_vertex_05002E50[];
    extern Vtx hoot_seg5_vertex_05002F78[];
    extern Vtx hoot_seg5_vertex_050030A0[];
    extern Vtx hoot_seg5_vertex_050031C8[];
    extern Vtx hoot_seg5_vertex_050032F0[];
    extern Vtx hoot_seg5_vertex_05003418[];
    extern Vtx hoot_seg5_vertex_05003540[];
    extern Vtx hoot_seg5_vertex_05003668[];
    extern Vtx yellow_sphere_seg5_vertex_05000000[];
    extern Vtx castle_courtyard_seg7_vertex_070021C0[];
    extern Vtx castle_courtyard_seg7_vertex_070022A0[];
    extern Vtx bbh_seg7_vertex_070076C0[];
    extern Vtx bbh_seg7_vertex_070077B0[];
    extern Vtx lll_seg7_vertex_07013830[];
    extern Vtx ttc_seg7_vertex_0700B238[];
    extern Vtx dirt_seg3_vertex_0302BDC8[];
    return ptr == mario_right_hand_cap_wings_half_1_dl_vertex ||
           ptr == mario_right_hand_cap_wings_half_2_dl_vertex ||
           ptr == mario_wings_half_1_dl_vertex ||
           ptr == mario_wings_half_2_dl_vertex ||
           ptr == hoot_seg5_vertex_05002E50 ||
           ptr == hoot_seg5_vertex_05002F78 ||
           ptr == hoot_seg5_vertex_050030A0 ||
           ptr == hoot_seg5_vertex_050031C8 ||
           ptr == hoot_seg5_vertex_050032F0 ||
           ptr == hoot_seg5_vertex_05003418 ||
           ptr == hoot_seg5_vertex_05003540 ||
           ptr == hoot_seg5_vertex_05003668 ||
           ptr == yellow_sphere_seg5_vertex_05000000 ||
           ptr == castle_courtyard_seg7_vertex_070021C0 ||
           ptr == castle_courtyard_seg7_vertex_070022A0 ||
           ptr == bbh_seg7_vertex_070076C0 ||
           ptr == bbh_seg7_vertex_070077B0 ||
           ptr == lll_seg7_vertex_07013830 ||
           ptr == ttc_seg7_vertex_0700B238 ||
           ptr == dirt_seg3_vertex_0302BDC8;
}

static void rom_asset_load_vtx(struct RomAsset* asset) {
    Vtx* vtx = asset->ptr;
    while (asset->cursor < asset->segmentedSize) {
        vtx->v.ob[0] = READ16(asset);
        vtx->v.ob[1] = READ16(asset);
        vtx->v.ob[2] = READ16(asset);
        vtx->v.flag  = READ16(asset);
        vtx->v.tc[0] = READ16(asset);
        vtx->v.tc[1] = READ16(asset);
        if (rom_asset_override_vertex_colors(asset->ptr)) {
            vtx->v.cn[0] = 0xFF;
            vtx->v.cn[1] = 0xFF;
            vtx->v.cn[2] = 0xFF;
            vtx->v.cn[3] = 0xFF;
            asset->cursor += sizeof(s8) * 4;
        } else {
            vtx->v.cn[0] = READ8(asset);
            vtx->v.cn[1] = READ8(asset);
            vtx->v.cn[2] = READ8(asset);
            vtx->v.cn[3] = READ8(asset);
        }
        vtx++;
    }
}

static u32 rom_dynlist_read_be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

bool rom_assets_decode_dynlist(struct DynList* destination, size_t commandCount,
        const u8* segment, size_t segmentSize, size_t offset, size_t byteCount,
        const struct RomDynlistBinding* bindings, size_t bindingCount) {
    if (!destination || !segment || commandCount < 2 ||
        commandCount > SIZE_MAX / sizeof(struct DynList) ||
        commandCount > SIZE_MAX / 24 || byteCount != commandCount * 24 ||
        offset > segmentSize || byteCount > segmentSize - offset ||
        bindingCount > commandCount * 2 || (bindingCount && !bindings)) return false;
    const u8* source = segment + offset;
    for (size_t i = 0; i < bindingCount; ++i) {
        const struct RomDynlistBinding* b = &bindings[i];
        if (b->commandIndex >= commandCount || (b->word != 1 && b->word != 2) ||
            !b->hostValue || b->romValue < 0x04000000u ||
            rom_dynlist_read_be32(source + b->commandIndex * 24 + b->word * 4) != b->romValue) return false;
        for (size_t j = 0; j < i; ++j)
            if (bindings[j].commandIndex == b->commandIndex && bindings[j].word == b->word) return false;
    }
    struct DynList* stage = calloc(commandCount, sizeof(*stage));
    if (!stage) return false;
    bool valid = true;
    for (size_t i = 0; i < commandCount && valid; ++i) {
        const u8* row = source + i * 24;
        u32 command = rom_dynlist_read_be32(row);
        if ((i == 0 && command != 53716) ||
            (i == commandCount - 1 && command != 58) ||
            (i > 0 && i < commandCount - 1 && command > 57)) { valid = false; break; }
        stage[i].cmd = (s32)command;
        for (u8 word = 1; word <= 2; ++word) {
            u32 value = rom_dynlist_read_be32(row + word * 4);
            void* native = (void*)(uintptr_t)value;
            bool bound = false;
            for (size_t j = 0; j < bindingCount; ++j)
                if (bindings[j].commandIndex == i && bindings[j].word == word) {
                    native = bindings[j].hostValue; bound = true; break;
                }
            // Original N64 addresses are never interpreted as host pointers.
            if (value >= 0x04000000u && !bound) { valid = false; break; }
            if (word == 1) stage[i].w1.ptr = native; else stage[i].w2.ptr = native;
        }
        u32 values[3];
        for (size_t j = 0; j < 3; ++j) values[j] = rom_dynlist_read_be32(row + 12 + j * 4);
        memcpy(&stage[i].vec.x, &values[0], sizeof(u32));
        memcpy(&stage[i].vec.y, &values[1], sizeof(u32));
        memcpy(&stage[i].vec.z, &values[2], sizeof(u32));
    }
    if (valid) memcpy(destination, stage, commandCount * sizeof(*stage));
    free(stage);
    return valid;
}

bool rom_assets_decode_u16(void* destination, size_t wordCount, const u8* segment, size_t segmentSize, size_t offset, size_t byteCount) {
    // Subtraction-based checks avoid overflow even for SIZE_MAX inputs.
    if (!destination || !segment || byteCount == 0 || (byteCount & 1) ||
        wordCount != byteCount / 2 || offset > segmentSize || byteCount > segmentSize - offset) {
        return false;
    }
    u8* output = (u8*)destination;
    const u8* input = segment + offset;
    for (size_t i = 0; i < wordCount; ++i) {
        u16 value = (u16)(((u16)input[i * 2] << 8) | input[i * 2 + 1]);
        memcpy(output + i * 2, &value, sizeof(value));
    }
    return true;
}

bool rom_assets_decode_light(void* destination, const u8* segment, size_t segmentSize, size_t offset) {
    if (!destination || !segment || offset > segmentSize || 24 > segmentSize - offset) {
        return false;
    }
    memcpy(destination, segment + offset, 24);
    return true;
}

static u32 rom_asset_gfx_word(const u8* input) {
    return ((u32)input[0] << 24) | ((u32)input[1] << 16) |
           ((u32)input[2] << 8) | input[3];
}

static bool rom_asset_gfx_command(Gfx* output, u32 w0, u32 w1) {
    // Original F3D wire opcodes are explicit: host GBI constants differ.
    output->words.w0 = w0;
    output->words.w1 = w1;
    switch (w0 >> 24) {
        case 0xe7: // RDP pipe sync
        case 0xe8: // RDP tile sync
            return (w0 & 0x00ffffff) == 0 && w1 == 0;
        case 0xfc: // RDP combine mode
        case 0xf5: // RDP tile descriptor
        case 0xf2: // RDP tile bounds
            return true;
        case 0xb6: // Only the verified flat-shading clear operation is supported.
            if (w0 != 0xb6000000 || w1 != 0x00000200) return false;
            gSPClearGeometryMode(output, G_SHADING_SMOOTH);
            return true;
        case 0xbb: // F3D texture scale/level/tile/enable -> active host GBI.
            if ((w0 & 0x00ffc0fe) != 0) return false;
            gSPTexture(output, w1 >> 16, w1 & 0xffff,
                       (w0 >> 11) & 7, (w0 >> 8) & 7, w0 & 1);
            return true;
        case 0xbc: // Only the verified one-directional-light configuration.
            if (w0 != 0xbc000002 || w1 != 0x80000040) return false;
            gSPNumLights(output, NUMLIGHTS_1);
            return true;
        case 0xb8:
            if (w0 != 0xb8000000 || w1 != 0) return false;
            gSPEndDisplayList(output);
            return true;
        default: // In particular: no vertices, textures, calls, or pointers.
            return false;
    }
}

bool rom_assets_decode_gfx(void* destination, size_t commandCount, const u8* segment, size_t segmentSize, size_t offset, size_t byteCount) {
    if (!destination || !segment || byteCount == 0 || byteCount % 8 != 0 ||
        commandCount != byteCount / 8 || commandCount > SIZE_MAX / sizeof(Gfx) ||
        offset > segmentSize || byteCount > segmentSize - offset) return false;
    const u8* input = segment + offset;
    Gfx command = { 0 };
    // Validate the whole list before modifying any native destination bytes.
    for (size_t i = 0; i < commandCount; ++i) {
        u32 w0 = rom_asset_gfx_word(input + i * 8);
        u32 w1 = rom_asset_gfx_word(input + i * 8 + 4);
        if (!rom_asset_gfx_command(&command, w0, w1) ||
            ((w0 >> 24) == 0xb8) != (i == commandCount - 1)) return false;
    }
    for (size_t i = 0; i < commandCount; ++i) {
        rom_asset_gfx_command(&command, rom_asset_gfx_word(input + i * 8),
                             rom_asset_gfx_word(input + i * 8 + 4));
        memcpy((u8*)destination + i * sizeof(Gfx), &command, sizeof(command));
    }
    return true;
}

static void rom_asset_load(struct RomAsset* asset) {
    if (!rom_asset_load_segment(asset->physicalAddress, asset->physicalSize)) {
        return;
    }
    if (asset->physicalAddress == 0x00396340 && asset->assetType == ROM_ASSET_TEXTURE && clock_is_date(4, 1)) {
        switch (asset->segmentedAddress) {
            case 0x00008000: memcpy(asset->ptr, apparition_texture_1, asset->segmentedSize); return;
            case 0x00008800: memcpy(asset->ptr, apparition_texture_2, asset->segmentedSize); return;
            case 0x00009000: memcpy(asset->ptr, apparition_texture_3, asset->segmentedSize); return;
            case 0x00009800: memcpy(asset->ptr, apparition_texture_4, asset->segmentedSize); return;
        }
    }
    switch (asset->assetType) {
        case ROM_ASSET_VTX:       rom_asset_load_vtx(asset); break;
        case ROM_ASSET_TEXTURE:   ROM_ASSET_LOAD_DATA(8);    break;
        case ROM_ASSET_SAMPLE:    ROM_ASSET_LOAD_DATA(8);    break;
        case ROM_ASSET_COLLISION: ROM_ASSET_LOAD_DATA(16);   break;
        case ROM_ASSET_ANIM:      ROM_ASSET_LOAD_DATA(16);   break;
        case ROM_ASSET_DIALOG:    ROM_ASSET_LOAD_DATA(8);    break;
        case ROM_ASSET_DEMO:      ROM_ASSET_LOAD_DATA(8);    break;
        case ROM_ASSET_GFX_SCALAR:
            if (!rom_assets_decode_gfx(asset->ptr, asset->segmentedSize / 8,
                                      sCurrentSegmentMemory, sCurrentSegmentSize,
                                      asset->segmentedAddress, asset->segmentedSize)) {
                LOG_ERROR("Invalid or unsupported scalar ROM display list: offset %u, bytes %u, segment %u",
                          asset->segmentedAddress, asset->segmentedSize, sCurrentSegmentSize);
            }
            break;
        case ROM_ASSET_LIGHT:
            if (asset->segmentedSize != 24 || !rom_assets_decode_light(asset->ptr,
                        sCurrentSegmentMemory, sCurrentSegmentSize, asset->segmentedAddress)) {
                LOG_ERROR("Invalid ROM light span: offset %u, segment %u",
                          asset->segmentedAddress, sCurrentSegmentSize);
            }
            break;
        case ROM_ASSET_DYNLIST:
            if (!rom_assets_decode_dynlist(asset->ptr, asset->segmentedSize / 24,
                    sCurrentSegmentMemory, sCurrentSegmentSize, asset->segmentedAddress,
                    asset->segmentedSize, asset->dynBindings, asset->dynBindingCount)) {
                LOG_ERROR("Invalid owned-ROM Goddard command table");
            }
            break;
        case ROM_ASSET_U16:
            if (!rom_assets_decode_u16(asset->ptr, asset->segmentedSize / 2,
                                      sCurrentSegmentMemory, sCurrentSegmentSize,
                                      asset->segmentedAddress, asset->segmentedSize)) {
                LOG_ERROR("Invalid structural ROM table span: offset %u, bytes %u, segment %u",
                          asset->segmentedAddress, asset->segmentedSize, sCurrentSegmentSize);
            }
            break;
        default:
            LOG_ERROR("Could not load unknown asset type %u!", asset->assetType);
    }
}

void rom_assets_load(void) {
    LOG_INFO("loading asset");

    assert(fs_sys_file_exists(gRomFilename)); // Should never be false

    sRomFile = fopen(gRomFilename, "rb");

    while (sRomAssets) {
        rom_asset_load(sRomAssets);

        struct RomAsset* next = sRomAssets->next;
        free(sRomAssets);
        sRomAssets = next;
    }

    if (sCurrentSegmentMemory) {
        free(sCurrentSegmentMemory);
        sCurrentSegmentMemory = NULL;
    }

    fclose(sRomFile);
}

void rom_assets_queue(void* ptr, enum RomAssetType assetType, u32 physicalAddress, u32 physicalSize, u32 segmentedAddress, u32 segmentedSize) {
    struct RomAsset* asset = (struct RomAsset*)calloc(1, sizeof(struct RomAsset));
    asset->ptr = ptr;
    asset->assetType = assetType;
    asset->physicalAddress = physicalAddress;
    asset->physicalSize = physicalSize;
    asset->segmentedAddress = segmentedAddress;
    asset->segmentedSize = segmentedSize;
    asset->cursor = 0;
    asset->next = sRomAssets;
    sRomAssets = asset;
    LOG_INFO("added asset");
}

void rom_assets_queue_dynlist(struct DynList* destination, u32 physicalAddress, u32 byteCount,
        const struct RomDynlistBinding* bindings, size_t bindingCount) {
    rom_assets_queue(destination, ROM_ASSET_DYNLIST, physicalAddress, byteCount, 0, byteCount);
    sRomAssets->dynBindings = bindings;
    sRomAssets->dynBindingCount = bindingCount;
}

u8* rom_assets_decompress(u32* data, u32* decompressedSize) {
    if (BSWAP32(data[0]) != 0x4d494f30) {
        return NULL;
    }

    // ripped from tools/gen_asset_list.cpp
    uint32_t* src = data;
    uint32_t size = BSWAP32(src[1]);
    u8* output = calloc(size, 1);
    char *dest = (char *)output;
    char *destEnd = (size + dest);
    uint16_t *cmpOffset = (uint16_t *)((char *)src + BSWAP32(src[2]));
    char *rawOffset = ((char *)src + BSWAP32(src[3]));
    int counter = 0;
    uint32_t controlBits;

    src += 4;

    while (dest != destEnd) {
        if (counter == 0) {
            controlBits = *src++;
            controlBits = BSWAP32(controlBits);
            counter = 32;
        }

        if (controlBits & 0x80000000) {
            *dest++ = *rawOffset++;
        }
        else {
            uint16_t dcmpParam = *cmpOffset++;
            dcmpParam = BSWAP16(dcmpParam);
            int dcmpCount = (dcmpParam >> 12) + 3;
            char* dcmpPtr = dest - (dcmpParam & 0x0FFF);

            while (dcmpCount) {
                *dest++ = dcmpPtr[-1];
                dcmpCount--;
                dcmpPtr++;
            }
        }

        counter--;
        controlBits <<= 1;
    }

    *decompressedSize = size;
    return output;
}
