#include "../physics/enemy_impact.h"
#include "RLConst.h"
#include "Sim/Car/CarConfig/CarConfig.h"
#include <cassert>
#include <cstdio>
using namespace RocketSim;
static_assert(ROCKET_ENEMY_SUPERSONIC_SPEED==RLConst::SUPERSONIC_START_SPEED*ROCKET_HOST_SCALE);
int main(){
    assert(ROCKET_ENEMY_HALF_LENGTH==CAR_CONFIG_OCTANE.hitboxSize.x*.5f*ROCKET_HOST_SCALE);
    assert(ROCKET_ENEMY_HALF_WIDTH==CAR_CONFIG_OCTANE.hitboxSize.y*.5f*ROCKET_HOST_SCALE);
    assert(ROCKET_ENEMY_HALF_HEIGHT==CAR_CONFIG_OCTANE.hitboxSize.z*.5f*ROCKET_HOST_SCALE);
    assert(ROCKET_ENEMY_OFFSET==CAR_CONFIG_OCTANE.hitboxPosOffset.x*ROCKET_HOST_SCALE);
    assert(ROCKET_ENEMY_OFFSET_UP==CAR_CONFIG_OCTANE.hitboxPosOffset.z*ROCKET_HOST_SCALE);
    puts("enemy vendor mapping: pinned RocketSim supersonic entry threshold and Octane collision box match");
}
