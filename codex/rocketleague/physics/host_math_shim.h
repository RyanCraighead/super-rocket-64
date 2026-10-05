#pragma once
#include <cmath>
// SM64 exports atan2f with its N64 yaw convention. RocketSim/Bullet require
// the standard Cartesian convention. Parse system headers first, then redirect
// only this dependency's float calls to the standard double function. No host
// symbol or pinned dependency source is modified.
inline float rocket_standard_atan2f(float y,float x) {
    return static_cast<float>(std::atan2(static_cast<double>(y),static_cast<double>(x)));
}
#define atan2f rocket_standard_atan2f
