#ifndef SM64_BK_POSE_H
#define SM64_BK_POSE_H
#include <array>
#include <string>
#include <vector>

namespace bk {
constexpr int BONE_COUNT = 0x6D;
/* Public layout is translation XYZ, Euler rotation XYZ in degrees, scale XYZ. */
using Transform = std::array<float, 9>;
using Matrix = std::array<float, 16>;
struct Bone { int parent, bone_id; std::array<float, 3> pivot; };
struct Key { int frame, value, flags; };
struct Track { int bone, channel; std::vector<Key> keys; };
struct Animation {
    int id, first_frame, last_frame, duration;
    std::string name;
    std::vector<Track> tracks;
};
struct Rig {
    float translation_scale = 1;
    std::vector<Bone> bones;
    std::vector<Animation> animations;
    /* Strict loader for local decoded bk-original-animation-v1 JSON. */
    static Rig load(const std::string &animations_json);
    /* Animation is its vector index. Frame is an absolute original source
     * frame, not elapsed seconds. -1,0 explicitly requests original bind pose.
     * The result always has 109 original bone-transform slots. */
    std::vector<Transform> sample(int animation, float frame) const;
    std::vector<Transform> sample_progress(int animation, float progress) const;
    /* Original parent*T(pivot+translation_scale*T)*Rz*Ry*Rx*S*T(-pivot).
     * Original vertices are already model-space. Results are column-major,
     * with bones.size() as an appended actor-identity matrix for source -1.
     * No billboard bones are used here; billboards duplicates joints to fit
     * the established renderer API. Host libm/matrices approximate the original
     * quaternion conversion, trig and RSP rounding; no cross-animation blending
     * is implied. view is validated but not multiplied into world matrices. */
    bool palettes(const std::vector<Transform> &pose, const float view[16],
                  const float world[3], float yaw_degrees, float host_scale,
                  float object_scale, std::vector<Matrix> &joints,
                  std::vector<Matrix> &billboards) const;
};
}
#endif
