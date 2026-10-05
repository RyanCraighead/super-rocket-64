#include "bk_pose.h"
#include "utils/json.hpp"
#include "utils/oot_asset_path.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <stdexcept>

/* Source: n64decomp/banjo-kazooie e8d31fa53645616fcac6e051543222b973a2edb0,
 * animationfile.c, code_B9770.c, code_630D0.c, code_BE2C0.c, core1/mlmtx.c.
 * Decoded assets supply every track and pivot; none are authored here. */
namespace bk {
namespace {
using Json = nlohmann::json;
void need(bool c, const char *s) { if (!c) throw std::runtime_error(s); }
int integer(const Json &j, int lo, int hi) {
    need(j.is_number_integer() && !j.is_boolean(), "invalid BK integer");
    if (j.is_number_unsigned()) need(hi >= 0 && j.get<uint64_t>() <= uint64_t(hi), "BK integer overflow");
    int64_t v = j.get<int64_t>();
    need(v >= lo && v <= hi, "BK integer outside bounds");
    return int(v);
}
float scalar(const Json &j, float limit = 100000) {
    need(j.is_number() && !j.is_boolean(), "invalid BK scalar");
    float v = j.get<float>();
    need(std::isfinite(v) && std::fabs(v) <= limit, "nonfinite or unbounded BK scalar");
    return v;
}
const Json &array(const Json &j, size_t lo, size_t hi) {
    need(j.is_array() && j.size() >= lo && j.size() <= hi, "invalid BK array");
    return j;
}
Matrix identity() { Matrix m = {}; m[0] = m[5] = m[10] = m[15] = 1; return m; }
Matrix multiply(const Matrix &a, const Matrix &b) {
    Matrix r = {};
    for (int c = 0; c < 4; ++c) for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 4; ++k) r[c*4+i] += a[k*4+i] * b[c*4+k];
    return r;
}
Matrix translation(float x, float y, float z) {
    Matrix m = identity(); m[12] = x; m[13] = y; m[14] = z; return m;
}
Matrix rotation(float x, float y, float z) {
    const float radians = float(3.141592654 / 180.0);
    x *= radians; y *= radians; z *= radians;
    Matrix rx = identity(), ry = identity(), rz = identity();
    rx[5] = rx[10] = std::cos(x); rx[6] = std::sin(x); rx[9] = -rx[6];
    ry[0] = ry[10] = std::cos(y); ry[8] = std::sin(y); ry[2] = -ry[8];
    rz[0] = rz[5] = std::cos(z); rz[1] = std::sin(z); rz[4] = -rz[1];
    return multiply(rz, multiply(ry, rx));
}
float catmull(float x, float a, float b, float c, float d) {
    x = std::max(0.f, std::min(1.f, x));
    // The source's double coefficient constants round once into f32 slots.
    float c2 = float(-.5*a + 1.5*b - 1.5*c + .5*d);
    float c1 = float(a - 2.5*b + 2.0*c - .5*d);
    float c0 = float(-.5*a + .5*c);
    return (((c2*x + c1)*x + c0)*x) + b;
}
float value(const Key &k) { return float(k.value) / 64.f; }
float evaluate(const Animation &a, const Track &t, float frame) {
    const Key &first = t.keys.front(), &last = t.keys.back();
    if (int(frame) < first.frame) {
        float base = t.channel >= 3 && t.channel <= 5 ? 1.f : 0.f;
        float after = (first.flags & 2) && t.keys.size() >= 2 ? value(t.keys[1]) : value(first);
        return catmull((frame-a.first_frame)/(first.frame-a.first_frame), base, base, value(first), after);
    }
    if (int(frame) >= last.frame) {
        float before = (last.flags & 1) && t.keys.size() >= 2 ? value(t.keys[t.keys.size()-2]) : value(last);
        return catmull(frame-last.frame, before, value(last), value(last), value(last));
    }
    size_t lo = 0, hi = t.keys.size()-1;
    while (hi-lo > 1) {
        size_t mid = (lo+hi)/2;
        if (t.keys[mid].frame <= int(frame)) lo = mid; else hi = mid;
    }
    const Key &left = t.keys[lo], &right = t.keys[hi];
    float weight = (frame-left.frame)/(right.frame-left.frame);
    float l = value(left), r = value(right);
    if (!(left.flags & 1) && !(right.flags & 2)) return l + (r-l)*weight;
    float before = (left.flags & 1) && lo > 0 ? value(t.keys[lo-1]) : l;
    float after = (right.flags & 2) && hi+1 < t.keys.size() ? value(t.keys[hi+1]) : r;
    return catmull(weight, before, l, r, after);
}
}
Rig Rig::load(const std::string &path) {
    auto bytes = oot_asset_path::readFile(oot_asset_path::canonical(path), 16*1024*1024);
    Json j = Json::parse(bytes.begin(), bytes.end(), [](int depth, Json::parse_event_t, Json &) {
        need(depth <= 24, "BK JSON nesting limit"); return true;
    });
    need(j.at("format") == "bk-original-animation-v1", "unsupported BK animation schema");
    Rig rig;
    const Json &s = j.at("skeleton");
    rig.translation_scale = scalar(s.at("translation_scale"), 1000);
    need(rig.translation_scale > 0, "invalid BK translation scale");
    const Json &bones = array(s.at("bones"), 1, BONE_COUNT);
    for (size_t i = 0; i < bones.size(); ++i) {
        const Json &b = bones[i];
        integer(b.at("index"), int(i), int(i));
        Bone bone;
        bone.parent = integer(b.at("parent"), -1, int(i)-1);
        bone.bone_id = integer(b.at("bone_id"), 0, BONE_COUNT-1);
        const Json &pivot = array(b.at("pivot"), 3, 3);
        for (int k = 0; k < 3; ++k) bone.pivot[k] = scalar(pivot[k]);
        rig.bones.push_back(bone);
    }
    std::set<int> ids;
    size_t totalKeys = 0;
    for (const Json &anim : array(j.at("animations"), 1, 128)) {
        Animation a;
        a.id = integer(anim.at("id"), 0, 0xFFFF);
        need(ids.insert(a.id).second, "duplicate BK animation ID");
        need(anim.at("name").is_string(), "invalid BK animation name");
        a.name = anim.at("name").get<std::string>();
        need(!a.name.empty() && a.name.size() <= 128, "unbounded BK animation name");
        a.first_frame = integer(anim.at("first_frame"), 0, 0x3FFF);
        a.last_frame = integer(anim.at("last_frame"), a.first_frame, 0x3FFF);
        a.duration = integer(anim.at("duration"), a.last_frame-a.first_frame, a.last_frame-a.first_frame);
        bool used[BONE_COUNT][9] = {};
        int previousBone = 1;
        for (const Json &track : array(anim.at("tracks"), 1, (BONE_COUNT-1)*9)) {
            Track t;
            t.bone = integer(track.at("bone"), previousBone, BONE_COUNT-1);
            previousBone = t.bone;
            t.channel = integer(track.at("channel"), 0, 8);
            need(!used[t.bone][t.channel], "duplicate BK animation channel");
            used[t.bone][t.channel] = true;
            int previousFrame = a.first_frame-1;
            const Json &keys = array(track.at("keys"), 1, 0x4000);
            totalKeys += keys.size();
            need(totalKeys <= 500000, "BK total key limit");
            for (const Json &key : keys) {
                array(key, 3, 3);
                Key k = {integer(key[0], previousFrame+1, a.last_frame),
                         integer(key[1], -32768, 32767), integer(key[2], 0, 3)};
                previousFrame = k.frame;
                t.keys.push_back(k);
            }
            a.tracks.push_back(t);
        }
        rig.animations.push_back(a);
    }
    return rig;
}
std::vector<Transform> Rig::sample(int animation, float frame) const {
    need(std::isfinite(frame), "nonfinite BK animation frame");
    const Transform neutral = {{0,0,0,0,0,0,1,1,1}};
    std::vector<Transform> result(BONE_COUNT, neutral);
    if (animation == -1) { need(frame == 0, "BK bind pose has no time"); return result; }
    need(animation >= 0 && size_t(animation) < animations.size(), "unknown BK animation index");
    const Animation &a = animations[animation];
    need(frame >= a.first_frame && frame <= a.last_frame, "BK frame outside original interval");
    static const int channel[] = {3,4,5,6,7,8,0,1,2};
    for (const Track &t : a.tracks) result[t.bone][channel[t.channel]] = evaluate(a, t, frame);
    return result;
}
std::vector<Transform> Rig::sample_progress(int animation, float progress) const {
    need(std::isfinite(progress) && progress >= 0 && progress <= 1, "BK progress outside interval");
    need(animation >= 0 && size_t(animation) < animations.size(), "unknown BK animation index");
    const Animation &a = animations[animation];
    return sample(animation, a.first_frame + progress * a.duration);
}
bool Rig::palettes(const std::vector<Transform> &pose, const float view[16],
                   const float world[3], float yaw, float hostScale, float objectScale,
                   std::vector<Matrix> &joints, std::vector<Matrix> &billboards) const {
    if (pose.size() != BONE_COUNT || bones.empty() || bones.size() > BONE_COUNT || !view || !world ||
        !std::isfinite(yaw) || !std::isfinite(hostScale) || !std::isfinite(objectScale) ||
        hostScale <= 0 || hostScale > 100 || objectScale <= 0 || objectScale > 100) return false;
    for (int k = 0; k < 16; ++k) if (!std::isfinite(view[k])) return false;
    for (int k = 0; k < 3; ++k) if (!std::isfinite(world[k])) return false;
    for (const Transform &t : pose) for (float v : t)
        if (!std::isfinite(v) || std::fabs(v) > 100000) return false;
    Matrix global = multiply(translation(world[0], world[1], world[2]), rotation(0, yaw, 0));
    float factor = hostScale*objectScale;
    for (int col = 0; col < 3; ++col) for (int row = 0; row < 3; ++row) global[col*4+row] *= factor;
    std::vector<Matrix> output;
    output.reserve(bones.size()+1);
    for (const Bone &b : bones) {
        const Transform &t = pose[b.bone_id];
        Matrix scale = identity(); scale[0] = t[6]; scale[5] = t[7]; scale[10] = t[8];
        Matrix local = multiply(translation(b.pivot[0]+translation_scale*t[0],
                                            b.pivot[1]+translation_scale*t[1],
                                            b.pivot[2]+translation_scale*t[2]),
                     multiply(rotation(t[3],t[4],t[5]),
                     multiply(scale, translation(-b.pivot[0],-b.pivot[1],-b.pivot[2]))));
        output.push_back(multiply(b.parent < 0 ? global : output[b.parent], local));
        for (float v : output.back()) if (!std::isfinite(v)) return false;
    }
    output.push_back(global);
    joints = output;
    billboards = output;
    return true;
}
}
