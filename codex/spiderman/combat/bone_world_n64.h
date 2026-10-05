#ifndef SMN64_COMBAT_BONE_WORLD_H
#define SMN64_COMBAT_BONE_WORLD_H
#include <stdint.h>
#include <stddef.h>
#include "../markers/markers_n64.h"
/* Exact post-pose0x8004A8F8 for the melee zero-offset marker. Unlike authored
 * marker lookup0x8004A720, this call never mirrors the body first column.
 * Translation is retained player17C/180/184, not recomputed from current pos. */
int smn64_combat_bone_world(uint16_t bone,const int16_t *poses,size_t pose_count,
    const int16_t body_matrix[9],const int32_t retained_translation[3],
    const int32_t position_fixed12[3],int32_t out[3]);
/* Same original4A8F8 with actual local offsets, used by trap-sheet mesh markers.
 * Return1 valid,0 invalid shape/dependency with output left intact. */
int smn64_combat_marker_world(const SmN64Marker *,const int16_t *poses,size_t pose_count,
    const int16_t body_matrix[9],const int32_t retained_translation[3],
    const int32_t position_fixed12[3],int32_t out[3]);
#endif
