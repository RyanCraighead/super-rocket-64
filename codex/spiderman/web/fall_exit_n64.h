#ifndef SMN64_WEB_FALL_EXIT_H
#define SMN64_WEB_FALL_EXIT_H
#include "../movement/n64_behavior.h"
#include <stddef.h>
/* Common falling animation block80091BEC..80091E04. All positions/velocities
 * retain signed fixed12/Y-down. Source660 is NOT jump variant1180. */
typedef struct SmN64FallExit {
    SmN64Anim anim;
    int32_t position_y,velocity_y,previous_velocity_y;
    int32_t launch_flag;       /*660: pending web-release animation*/
    int32_t falling_origin_y;  /*112C*/
    uint32_t falling_tick;     /*1130, retained across non-triggering frames*/
    uint32_t now;              /*global800F965C*/
    int32_t d20,d24;
} SmN64FallExit;
/* Invoke in the original FALLING dispatcher, after landing and source-ordered
 * wall/ceiling/air-attack/swing/zip routes (and the optional damage branch),
 * before the common post-AI tail. Earlier successful actions skip this block.
 * Replace the bounded player's crossed-sign animation block with this helper;
 * do not run both. Authored successors have already run for this frame.
 * Source175/176 still record fall origin/time but retain660 and their clips.
 * Returns1 handled, or-1 invalid selected clip/count without any mutation. */
int smn64_web_fall_exit(SmN64FallExit *,const uint16_t *counts,size_t count);
#endif
