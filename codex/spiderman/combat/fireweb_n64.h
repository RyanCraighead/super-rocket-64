#ifndef SMN64_FIREWEB_H
#define SMN64_FIREWEB_H
#include "impact_web_n64.h"
/* Source target and graph boundaries, not generic nearest-target auto-aim. */
typedef struct SmN64FireActor {
    uint32_t id,flags_3c8;
    int16_t health;
    uint16_t type;
    int32_t position[3];
    uint32_t special_enabled;
} SmN64FireActor;
typedef struct SmN64FireSurface {
    uint32_t surface_present,hit,flags,special_actor;
    int32_t position[3];
    int16_t normal[3];
} SmN64FireSurface;
typedef struct SmN64FireWeb {
    int32_t position[3],forward[3];
    uint32_t target_id,graphic_id,special_point_id;
    uint16_t animation,graphic_hand;
    uint8_t attack_mode,loop_sound;
} SmN64FireWeb;
typedef struct SmN64FireEvent {
    uint32_t result_flags; /*1 resource failure,2 actor response,4 switch,8 rejected yank*/
    uint32_t update_graphic,graphic_id,attach_actor,release_graphic;
    int32_t muzzle[3],target[3];
    int16_t normal[3],angles[3];
    uint32_t surface_attached,actor_message,actor_message_target;
    uint32_t trap_samples,trap_web_type,yank_mark,activate_special;
    uint32_t spawn_impact,impact_damage,impact_lifetime,impact_speed,impact_special;
    uint32_t special_web_type,sound,loop_sound,sound_positional;
    SmN64WebResourceEvent resource;
    /* Source61270 rejection: resolve this actor's +10C attached sheet handle
     * and call sheet-release B0D30 BEFORE releasing the player's graphic.
     * Zero means no actor recipient; it is not a graphic handle itself. */
    uint32_t rejected_yank_actor;
} SmN64FireEvent;
typedef struct SmN64FireHost {
    void *context;
    int (*actor)(void *,uint32_t id,SmN64FireActor *);
    int (*bone)(void *,uint8_t bone,int32_t position[3]); /*exactly1 success*/
    /* source800A0EB0: maxdistance3072,minfacing2896,weights4096/4096;
     * specific interactable type0x197, explicit point position returned */
    int (*special_target)(void *,int32_t position[3],uint32_t *id);
    /* Source query with both include-special/target flags enabled. Mapping
     * special_actor must honor surface0x200 or the original level28 exception. */
    int (*trace)(void *,const int32_t from[3],const int32_t to[3],SmN64FireSurface *);
} SmN64FireHost;
/* Source800A8EEC aiming. Returns horizontal distance or-1 for source division
 * trap inputs. Empty coincident target preserves original odd-angle result. */
int32_t smn64_fireweb_angles(const int32_t from[3],const int32_t to[3],int16_t angles[3]);
/* Full target resolution and action dispatch at8009E73C. Existing-graphic actions
 * emit source actor message5(trap) or6(yank), eight trap-sheet samples per call,
 * and the source bit8 yank mark. These events must be committed to actual actors
 * before another AI tick; event emission alone does not trap or damage an enemy.
 * Impact muzzle/angles/damage50/lifetime30or120 feed impact_web_n64.
 * An unavailable pose/actor callback returns-2 and may follow original resource
 * spending. Preflight required host support; on failure stop or roll back the
 * whole owner/inventory/RNG, never blindly retry only this local state. */
int smn64_fireweb(SmN64FireWeb *,uint8_t auto_target,int32_t amount,
    const int32_t explicit_target[3],uint8_t surface_attached,const int16_t normal[3],
    SmN64WebResource *,uint32_t rng[3],const SmN64FireHost *,SmN64FireEvent *);
#endif
