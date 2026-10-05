#ifndef SM64_ROCKET_ADAPTER_H
#define SM64_ROCKET_ADAPTER_H
struct MarioState;
struct Object;
struct RocketSnapshot;
struct Surface;
int rocket_adapter_platform_contact(struct MarioState *m,struct Surface *floor,float height);
int rocket_adapter_interaction_snapshot(struct RocketSnapshot *state);
/* Local ownership/pose query for native collision detection; input capture
 * does not make a stationary car immune to enemies. Never steps physics. */
int rocket_adapter_body_snapshot(struct Object *object,struct RocketSnapshot *state);
int rocket_adapter_platform_snapshot(struct RocketSnapshot *state);
int rocket_adapter_car_selected(void);
void rocket_adapter_forget_platform(struct Object *object);
int rocket_adapter_object_visible(const float from[3],struct Object *object);
int rocket_adapter_whomp_path_clear(const float from[3],const float to[3],struct Object *object);
/* Enemy cylinder visibility, with this actor's authoritative cap lease. */
int rocket_adapter_enemy_visible(const float from[3],struct Object *object,unsigned verifiedCaps);
int rocket_adapter_update(struct MarioState *m);
void rocket_adapter_prepare_interactions(struct MarioState *m);
int rocket_adapter_vanish_switch_contact(struct Object *object);
int rocket_adapter_pickup_pose(struct RocketSnapshot *pose);
int rocket_adapter_cap_pickup_contact(const struct RocketSnapshot *pose,struct Object *object,unsigned verifiedCaps);
int rocket_adapter_cap_box_pose_contact(const struct RocketSnapshot *pose,struct Object *object,unsigned verifiedCaps);
int rocket_adapter_cap_box_contact(struct Object *object);
int rocket_adapter_vanish_box_contact(struct Object *object);
void rocket_adapter_suspend(void);
void rocket_adapter_set_selected(int selected);
const char *rocket_adapter_switch_reason(void);
#endif
