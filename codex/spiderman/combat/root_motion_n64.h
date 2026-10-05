#ifndef SMN64_COMBAT_ROOT_MOTION_H
#define SMN64_COMBAT_ROOT_MOTION_H
#include "combat_n64.h"
typedef struct SmN64CombatRoot {
    int32_t position[3],forward[3],right[3],up[3];
    int32_t retained_markers[4][3]; /* authored marker6,5,1,0, at prior AI tail */
} SmN64CombatRoot;
typedef struct SmN64RootHost {
    void *context;
    int (*refresh_pose)(void *,const SmN64Combo *);
    int (*actor_sweep)(void *,const int32_t from[3],const int32_t to[3],int32_t radius);
    int (*world_trace)(void *,const int32_t from[3],const int32_t to[3]);
    int (*marker)(void *,uint8_t authored_marker,int32_t world[3]);
} SmN64RootHost;
/* Exact9FF70–A05F0 ordering: pose refresh, actor query,three world rays,
 * position subtraction. Return1applied,0blocked,-1bad,-2missing callback/data.
 * Use inside combo_tick_resolved; pose must refer to that pre-input source clip.
 * Callback conventions1hit/0clear for queries,1success for pose/marker. */
int smn64_combo_root_resolve(SmN64CombatRoot *,const SmN64Combo *,const SmN64CombatMotion *,const SmN64RootHost *);
/* Call after original AI-tail body/pose refresh932C0/932E4 whilecombo.active.
 * This captures source markers6,5,1,0; it doesn't refresh them itself. */
int smn64_combo_root_retain(SmN64CombatRoot *,const SmN64RootHost *);
#endif
