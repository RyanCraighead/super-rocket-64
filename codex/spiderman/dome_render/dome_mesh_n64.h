#ifndef SMN64_DOME_MESH_N64_H
#define SMN64_DOME_MESH_N64_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Struct layout deliberately matches source Vtx and combat's SmN64DomeVertex.
 * Share owned pools through an explicit adapter or memcpy; don't type-pun. */
typedef struct SmN64DomeRenderVertex {
    int16_t x,y,z,flag,s,t; uint8_t rgba[4];
} SmN64DomeRenderVertex;
typedef struct SmN64DomeCorner { uint16_t pool; int16_t s,t; uint16_t matrix; } SmN64DomeCorner;
#define SMN64_DOME_MAX_VERTICES 127u
#define SMN64_DOME_MAX_CORNERS 450u
typedef struct SmN64DomeGeometry {
    uint16_t model_slot,node,texture_slot,vertex_count,corner_count;
    SmN64DomeRenderVertex vertices[SMN64_DOME_MAX_VERTICES];
    SmN64DomeCorner corners[SMN64_DOME_MAX_CORNERS];
} SmN64DomeGeometry;
typedef struct SmN64DomeDraw {
    uint16_t model_slot,node,texture_slot,corner_count;
    SmN64DomeRenderVertex vertices[SMN64_DOME_MAX_CORNERS];
    int32_t model_s16_16[16];
} SmN64DomeDraw;
/* Pure bounded decoder, no I/O. Caller must hash-check the original files using
 * export_assets' pinned manifest before calling: structural validity is not ROM
 * identity. Rejects unsupported model/node, count, index, matrix and bad sizes.
 * Entire output remains untouched on failure. No retained caller pointers. */
int smn64_dome_geometry_decode(uint16_t model_slot,uint16_t node,
    const uint8_t *vertices_be,size_t vertex_bytes,
    const uint8_t *corners_be,size_t corner_bytes,SmN64DomeGeometry *out);
/* Copies the caller's current mutable pool into face-expanded source geometry,
 * restoring corner-specific G_MODIFYVTX ST and retaining exact current RGBA.
 * No simulation/fade/scroll, no mesh clone and no original-pool mutation occurs.
 * Matrix must be source-produced native render-space signed16.16 column-major.
 * Material (ENV, fog, tile scroll, render mode) is a separate ordered contract. */
int smn64_dome_mesh_submit(const SmN64DomeGeometry *,
    const SmN64DomeRenderVertex *current_pool,size_t current_count,
    const int32_t model_s16_16[16],SmN64DomeDraw *out);
/* Exact zero-angle, non-flipped dome instance branch of57818, then guMtxF2L.
 * Original dome constructors never author rotation; nonzero rotations require
 * the general instance producer, not this bounded function. Flags bit0x200
 * controls application of signed fixed12 actor scales. Position is fixed12.
 * Native mesh scale1/16; host reflection(Y,Z)×16 is a separate renderer policy.
 * This is not the animated player mesh's x8 skeletal scale. */
int smn64_dome_zero_rotation_matrix(const int32_t position_fixed12[3],
    uint16_t actor_flags,const int16_t scale_fixed12[3],int32_t out[16]);
/* Explicit host finite-perspective embedding of original transparent pass
 * CE0D4(camera,1): scale left/right/bottom/top/near by float32(1.05), retain
 * far. Host projection reconstruction is approximate; source frustum and
 * fixed-matrix quantization remain external. General/orthographic inputs reject. */
int smn64_dome_host_projection(const float projection[16],float out[16]);
#ifdef __cplusplus
}
#endif
#endif
