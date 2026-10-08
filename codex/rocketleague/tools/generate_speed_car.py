"""Adapt exact verified RocketSim sites without modifying its pinned sources."""
from pathlib import Path
import sys

def generate(source):
    sites = [
        ("GetUpDir() * mutatorConfig.jumpImmediateForce * UU_TO_BT", "GetUpDir() * mutatorConfig.jumpImmediateForce * rocket_host_car_jump_impulse(this) * UU_TO_BT", 1),
        ("GetUpDir() * JUMP_IMMEDIATE_FORCE * UU_TO_BT", "GetUpDir() * JUMP_IMMEDIATE_FORCE * rocket_host_car_jump_impulse(this) * UU_TO_BT", 1),
        ("GetUpDir() * mutatorConfig.jumpAccel;", "GetUpDir() * mutatorConfig.jumpAccel * rocket_host_car_jump_hold(this);", 1),
        ("float absForwardSpeed_UU = abs(forwardSpeed_UU);", "const float speedScale = rocket_host_car_speed(this) * rocket_host_car_ground_mobility(this);\n\tfloat absForwardSpeed_UU = abs(forwardSpeed_UU) / speedScale;", 1),
        ("* driveSpeedScale;", "* driveSpeedScale * speedScale;", 1),
        ("realBrake * (BRAKE_TORQUE_AMOUNT * UU_TO_BT);", "realBrake * (BRAKE_TORQUE_AMOUNT * UU_TO_BT) * speedScale;", 1),
        ("if (baseFriction > 5)", "if (baseFriction > 5 * speedScale)", 1),
        ("float speedSquared = (_rigidBody.m_linearVelocity * BT_TO_UU).length2();", "const float speedScale = rocket_host_car_speed(this);\n\t\tfloat speedSquared = (_rigidBody.m_linearVelocity * BT_TO_UU).length2() / (speedScale * speedScale);", 1),
        ("(CAR_MAX_SPEED * UU_TO_BT)", "(CAR_MAX_SPEED * rocket_host_car_speed(this) * UU_TO_BT)", 3),
        ("(_internalState.isOnGround ? mutatorConfig.boostAccelGround : mutatorConfig.boostAccelAir) * UU_TO_BT", "(_internalState.isOnGround ? mutatorConfig.boostAccelGround : mutatorConfig.boostAccelAir) * rocket_host_car_speed(this) * UU_TO_BT", 1),
        ("controls.throttle * THROTTLE_AIR_ACCEL * UU_TO_BT", "controls.throttle * THROTTLE_AIR_ACCEL * rocket_host_car_speed(this) * UU_TO_BT", 1),
        ("abs(forwardSpeed_UU) / CAR_MAX_SPEED;", "abs(forwardSpeed_UU) / (CAR_MAX_SPEED * rocket_host_car_speed(this));", 1),
        ("abs(forwardSpeed_UU) < 100.0f", "abs(forwardSpeed_UU) < 100.0f * rocket_host_car_speed(this)", 1),
        ("dodgeDir * FLIP_INITIAL_VEL_SCALE;", "dodgeDir * FLIP_INITIAL_VEL_SCALE * rocket_host_car_speed(this);", 1),
    ]
    for old, new, count in sites:
        if source.count(old) != count:
            raise ValueError("Pinned car speed site changed: " + old)
        source = source.replace(old, new)
    return 'extern "C" float rocket_host_car_ground_mobility(const void *car);\nextern "C" float rocket_host_car_speed(const void *car);\nextern "C" float rocket_host_car_jump_impulse(const void *car);\nextern "C" float rocket_host_car_jump_hold(const void *car);\n' + source

if __name__ == "__main__":
    source, output = map(Path, sys.argv[1:])
    output.write_text(generate(source.read_text()), newline="\n")
