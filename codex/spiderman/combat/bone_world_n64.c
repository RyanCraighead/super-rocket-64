#include "bone_world_n64.h"
#include "../markers/markers_n64.h"
int smn64_combat_bone_world(uint16_t bone,const int16_t *poses,size_t pose_count,
    const int16_t body[9],const int32_t translation[3],const int32_t position[3],int32_t out[3]){
    const SmN64Marker m={{0,0,0},bone};
    return smn64_marker_world(&m,1,0,poses,pose_count,body,translation,position,0,out);
}

int smn64_combat_marker_world(const SmN64Marker *m,const int16_t *poses,size_t pose_count,
    const int16_t body[9],const int32_t translation[3],const int32_t position[3],int32_t out[3]){
    return smn64_marker_world(m,1,0,poses,pose_count,body,translation,position,0,out);
}
