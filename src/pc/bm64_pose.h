#ifndef SM64_BM64_POSE_H
#define SM64_BM64_POSE_H
#include <array>
#include <string>
#include <vector>
#include "utils/json.hpp"

namespace bm64 {
using Transform = std::array<float,9>;
using Matrix = std::array<float,16>;
struct Limb { int parent, child, sibling; Transform bind; };
struct Override { int limb; Transform value; };
struct Track {
    int opcode, start, end, pose;
    std::vector<int> poses, durations;
    std::vector<std::pair<int,float>> keys;
};
struct Animation { int id, duration; std::vector<Track> tracks; };
struct Rig {
    int root=0;
    std::vector<Limb> limbs;
    std::vector<int> order;
    std::vector<std::vector<Override>> poses;
    std::vector<Animation> animations;
    /* Throws on malformed, nonfinite, unbounded or disconnected source data. */
    static Rig load(const std::string &animations_json);
    /* animation=-1 explicitly requests the original stored bind pose. */
    std::vector<Transform> sample(int animation, float frame) const;
    std::vector<Transform> sample_layers(const std::vector<std::pair<int,float>> &channels) const;
    /* Original T*Rz*Rx*Ry (degrees), limb scale omitted by original draw path.
     * Host uses a uniform world scale; billboards cancel source orientation and
     * camera rotation while preserving center and object scale. Host libm and
     * float matrices approximate the original trig/RSP rounding. */
    bool palettes(const std::vector<Transform> &pose, const float view[16],
                  const float world[3], float yaw_degrees, float host_scale,
                  float object_scale, std::vector<Matrix> &joints,
                  std::vector<Matrix> &billboards) const;
};
}
#endif
