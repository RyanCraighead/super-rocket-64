#ifndef SM64_ROCKET_RUNTIME_H
#define SM64_ROCKET_RUNTIME_H
#include "../../codex/rocketleague/physics/rocket_physics.h"
#include "../../codex/rocketleague/input/gamepad.h"
#ifdef __cplusplus
extern "C" {
#endif
int rocket_runtime_init(void);
void rocket_runtime_shutdown(void);
int rocket_runtime_enabled(void);
int rocket_runtime_set_boost_mode(int mode);
int rocket_runtime_boost_mode(void);
/* Call once after local native pickup/offline or an authority-approved grant. */
int rocket_runtime_collect_coin(void);
void rocket_runtime_set_quicksand_depth(float depth);
int rocket_runtime_pole_release(const RocketSnapshot *pose,int jumped);
uint32_t rocket_runtime_epoch(void);
int rocket_runtime_rule_ready(void);
/* Retire a real online selection's contacts/grants without refilling its tank. */
void rocket_runtime_selection_changed(void);
int rocket_runtime_draw_snapshot(const RocketSnapshot *snapshot,const float view[16],const float projection[16],const int viewport[4]);
/* Native MARIO_SPECIAL_CAPS flags. Remote visuals never grant local physics. */
int rocket_runtime_draw_snapshot_caps(const RocketSnapshot *snapshot,uint32_t native_flags,const float view[16],const float projection[16],const int viewport[4]);
/* Visual-only native squash; the existing CNET pose/wire format stays unchanged. */
int rocket_runtime_draw_snapshot_player(const RocketSnapshot *snapshot,unsigned player_index,uint32_t native_flags,const float view[16],const float projection[16],const int viewport[4]);
void rocket_runtime_set_cap_visuals(uint32_t native_flags);
int rocket_runtime_owns_controls(void);
void rocket_runtime_gamepad(const RocketGamepad *pad);
void rocket_runtime_last_input(RocketInput *input);
/* Read current input for host interactions without stepping physics. */
int rocket_runtime_read_input(const RocketInput *keyboard,RocketInput *input);
/* Caller must verify local car selection/native action. Keeps focus/UI gates
 * while native damage temporarily owns movement and the car is not drawable. */
int rocket_runtime_read_selected_input(const RocketInput *keyboard,RocketInput *input);
void rocket_runtime_suspend(void);
void rocket_runtime_interrupt(void);
void rocket_runtime_set_water(int present,float level,int metal);
void rocket_runtime_set_water_current(const float velocity[3]);
void rocket_runtime_set_water_query(RocketWaterQuery query);
int rocket_runtime_set_environment(const RocketEnvironment *environment);
void rocket_runtime_set_metal_water(int active);
int rocket_runtime_mesh(int layer,const RocketTriangle *triangles,size_t count);
int rocket_runtime_platforms(const RocketPlatform *platforms,size_t count);
int rocket_runtime_reset(const float position[3],const float velocity[3],float yaw);
int rocket_runtime_recover(const RocketSnapshot *clear_pose);
int rocket_runtime_frame(uint64_t frame,const RocketInput *input,int paused,int blocked);
int rocket_runtime_bump(const float delta_velocity[3]);
int rocket_runtime_snapshot(RocketSnapshot *snapshot);
int rocket_runtime_draw(const float view[16],const float projection[16],const int viewport[4]);
const char *rocket_runtime_status(void);
#ifdef __cplusplus
}
#endif
#endif
