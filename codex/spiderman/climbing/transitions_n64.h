#ifndef SMN64_CLIMB_TRANSITIONS_H
#define SMN64_CLIMB_TRANSITIONS_H
#include "climbing_n64.h"
/* Pose-marker callback used by authored transition states. refresh_pose=1 is the
 * original82790 frame-pose/body-translation refresh; 0 retains owner's pose. */
typedef int (*SmN64ClimbPoseMarker)(void *, const SmN64ClimbState *, unsigned,
                                  int refresh_pose, int32_t out[3]);
typedef int (*SmN64ClimbClearance)(void *, int32_t position[3],
                                 const SmN64ClimbBasis *, int32_t distance);
typedef int (*SmN64ClimbAction)(void *, SmN64ClimbState *, uint32_t source_address);
typedef struct SmN64ClimbTransitionEnv {
    SmN64ClimbPoseMarker marker;
    SmN64ClimbClearance clearance;
    SmN64ClimbTrace trace;
    void *context;
    int32_t camera_special; /* playerA34, inhibits original yaw camera requests */
    SmN64ClimbAction action; /* optional pure state action; original source order */
} SmN64ClimbTransitionEnv;
typedef struct SmN64ClimbTransitionEvent {
    uint32_t camera_flags; /* 1=marker target,2=yaw request,4=profile request */
    int32_t camera_target[3];
    uint16_t camera_ticks, camera_yaw;
    int32_t camera_profile; /* 0 floor,1 wall,2 ceiling; source duration16 */
} SmN64ClimbTransitionEvent;
/* Source9821C/985A8/9801C entrypoints. Zero gates reject. Negative leaves
 * state/event unchanged. Positive performs the authored transition request. */
int smn64_climb_corner_begin(SmN64ClimbState *, int outer,
                            const SmN64ClimbTransitionEnv *, SmN64ClimbTransitionEvent *,
                            const uint16_t *, size_t);
int smn64_climb_ledge_begin(SmN64ClimbState *, const SmN64ClimbTransitionEnv *,
                           SmN64ClimbTransitionEvent *, const uint16_t *, size_t);
/* Source1000/2000/80000 states after original animation successor handling.
 * finished_clip=65535 means no successor was applied this tick. */
int smn64_climb_transition_step(SmN64ClimbState *, uint16_t finished_clip,
                               const SmN64ClimbTransitionEnv *, const uint16_t *, size_t);
#endif
