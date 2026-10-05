/* One host authority for native Wing/Metal/Vanish flags and their shared timer. */
#include "rocket_caps.h"
#include "rocket_wing.h"
#include "rocket_adapter.h"
#include "sm64.h"
#include "area.h"
#include "mario.h"
#include "level_update.h"
#include "object_list_processor.h"
#include "object_fields.h"
#include "behavior_data.h"
#include "behavior_table.h"
#include "course_table.h"
#include "level_table.h"
#include "interaction.h"
#include "hardcoded.h"
#include "sound_init.h"
#include "audio/external.h"
#include "pc/rocket_boost.h"
#include "pc/rocket_runtime.h"
#include "pc/character_net.h"
#include "pc/network/network.h"
#include "pc/utils/misc.h"
#include <math.h>
#include <string.h>

#define CAP_STATE_BYTES 23
#define CANCEL_BYTES 15
typedef struct CapLease {
    u32 revision, grant;
    u16 remaining, area;
    u8 global, paused, flags;
    double received;
} CapLease;
static CapLease leases[MAX_PLAYERS];
/* Entry grants belong to the native area epoch, never to incoming cap flags. */
static struct { u16 area; u8 global, seen; } entries[MAX_PLAYERS];
static u32 nextRevision, nextGrant;
static unsigned heartbeat;
static void grant_course_entry(unsigned i);
static u32 cap_flag(const BehaviorScript *b) {
    return b == bhvWingCap ? MARIO_WING_CAP : b == bhvMetalCap ? MARIO_METAL_CAP : b == bhvVanishCap ? MARIO_VANISH_CAP : 0;
}
static u16 cap_duration(u32 flag) {
    return flag == MARIO_WING_CAP ? gLevelValues.wingCapDuration :
        flag == MARIO_METAL_CAP ? gLevelValues.metalCapDuration : gLevelValues.vanishCapDuration;
}
static void cap_music(u32 flags) {
    u8 sequence = flags & MARIO_METAL_CAP ? gLevelValues.metalCapSequence :
        flags & MARIO_WING_CAP ? gLevelValues.wingCapSequence : gLevelValues.vanishCapSequence;
    play_cap_music(SEQUENCE_ARGS(4, sequence));
}
static int online(void) { return gCLIOpts.characterNet && !gCLIOpts.offline && gNetworkType != NT_NONE; }
static int native_pause(const struct MarioState *m, u16 remaining) {
    return remaining > 60 && (m->action == ACT_READING_AUTOMATIC_DIALOG ||
        m->action == ACT_READING_NPC_DIALOG || m->action == ACT_READING_SIGN || m->action == ACT_IN_CANNON);
}
static int alive(const struct MarioState *m) {
    if (!m || !m->marioObj || !m->area || m->health < 0x100 || !m->action) return 0;
    switch (m->action) {
        case ACT_BUBBLED: case ACT_STANDING_DEATH: case ACT_QUICKSAND_DEATH:
        case ACT_DEATH_ON_STOMACH: case ACT_DEATH_ON_BACK: case ACT_WATER_DEATH:
        case ACT_DEATH_EXIT: case ACT_UNUSED_DEATH_EXIT: case ACT_FALLING_DEATH_EXIT:
        case ACT_SPECIAL_DEATH_EXIT: return 0;
        default: return 1;
    }
}
static int same_area(unsigned i) {
    if (i >= MAX_PLAYERS || !gNetworkPlayerLocal) return 0;
    const struct NetworkPlayer *n = &gNetworkPlayers[i], *l = gNetworkPlayerLocal;
    return n->connected && n->currAreaSyncValid && n->currLevelSyncValid &&
        (i == 0 || n->currPositionValid) && n->currCourseNum == l->currCourseNum &&
        n->currActNum == l->currActNum && n->currLevelNum == l->currLevelNum && n->currAreaIndex == l->currAreaIndex;
}
static int current_lease(unsigned i) {
    CapLease *w = &leases[i];
    return same_area(i) && w->global == gNetworkPlayers[i].globalIndex &&
        w->area == gNetworkPlayers[i].currLevelAreaSeqId;
}
int rocket_caps_managed(const struct MarioState *m) {
    return online() && m && m->playerIndex < MAX_PLAYERS &&
        current_lease(m->playerIndex) && leases[m->playerIndex].remaining && leases[m->playerIndex].flags;
}
uint16_t rocket_caps_remaining(unsigned i) {
    if (i >= MAX_PLAYERS || !alive(&gMarioStates[i])) return 0;
    return online() ? rocket_caps_managed(&gMarioStates[i]) ? leases[i].remaining : 0 : gMarioStates[i].capTimer;
}
uint32_t rocket_caps_active_flags(unsigned i) {
    if (!rocket_caps_remaining(i)) return 0;
    const struct MarioState *m = &gMarioStates[i];
    if (!(m->flags & MARIO_CAP_ON_HEAD)) return 0;
    return m->flags & (online() ? leases[i].flags : MARIO_SPECIAL_CAPS);
}
uint32_t rocket_caps_visual_flags(unsigned i) {
    if (i >= MAX_PLAYERS) return 0;
    u32 flags = (gMarioStates[i].flags & ~MARIO_SPECIAL_CAPS) | rocket_caps_active_flags(i);
    u16 timer = rocket_caps_remaining(i);
    if (timer < 64 && ((UINT64_C(0x4444449249255555) >> timer) & 1)) {
        flags &= ~MARIO_SPECIAL_CAPS;
        if (!(flags & MARIO_CAPS)) flags &= ~MARIO_CAP_ON_HEAD;
    }
    return flags;
}
void rocket_caps_before_mario_update(struct MarioState *m) {
    if (!online() || !m || m->playerIndex >= MAX_PLAYERS) return;
    grant_course_entry(m->playerIndex);
    /* Preserve native removals; only filter unearned additions before physics. */
    u32 allowed = rocket_caps_managed(m) && alive(m) ? leases[m->playerIndex].flags : 0;
    m->flags &= ~MARIO_SPECIAL_CAPS | allowed;
}
static void remove_caps(unsigned i) {
    struct MarioState *m = &gMarioStates[i];
    if ((m->flags & MARIO_SPECIAL_CAPS) && i == 0) stop_cap_music();
    m->flags &= ~MARIO_SPECIAL_CAPS;
    m->capTimer = 0;
    leases[i].flags = 0;
    if (!(m->flags & MARIO_CAPS)) m->flags &= ~(MARIO_CAP_ON_HEAD | MARIO_CAP_IN_HAND);
}
void rocket_caps_clear(unsigned i) {
    if (i >= MAX_PLAYERS) return;
    if (leases[i].remaining) remove_caps(i);
    memset(&leases[i], 0, sizeof leases[i]);
    memset(&entries[i], 0, sizeof entries[i]);
    rocket_wing_topper_clear(i);
}
void rocket_caps_clear_all(void) {
    for (unsigned i = 0; i < MAX_PLAYERS; ++i) rocket_caps_clear(i);
    heartbeat = 0;
    /* Keep counters monotonic across area/disconnect clears within a session. */
}
void rocket_caps_apply(struct MarioState *m) {
    if (!online() || !m || m->playerIndex >= MAX_PLAYERS) return;
    if (rocket_caps_managed(m) && alive(m)) {
        m->flags = (m->flags & ~MARIO_SPECIAL_CAPS) | leases[m->playerIndex].flags | MARIO_CAP_ON_HEAD;
        m->flags &= ~MARIO_CAP_IN_HAND;
        m->capTimer = leases[m->playerIndex].remaining;
    } else {
        m->flags &= ~MARIO_SPECIAL_CAPS;
    }
}
static void put32(u8 *p, u32 n) { for (int k = 0; k < 4; ++k) p[k] = n >> (8*k); }
static u32 get32(const u8 *p) { return (u32)p[0] | (u32)p[1]<<8 | (u32)p[2]<<16 | (u32)p[3]<<24; }
static u64 get64(const u8 *p) { return (u64)get32(p) | (u64)get32(p+4)<<32; }
static void send_state(unsigned i) {
    if (gNetworkType != NT_SERVER || !online() || !gNetworkPlayers[i].connected) return;
    CapLease *w = &leases[i];
    w->revision = ++nextRevision; if (!w->revision) w->revision = ++nextRevision;
    w->global = gNetworkPlayers[i].globalIndex;
    w->area = gNetworkPlayers[i].currLevelAreaSeqId;
    u8 wire[CAP_STATE_BYTES] = {0};
    u64 session = rocket_boost_session_id();
    if (!session) return;
    put32(wire, session); put32(wire+4, session>>32);
    put32(wire+8, w->revision); put32(wire+12, w->grant);
    wire[16] = w->area; wire[17] = w->area>>8;
    wire[18] = w->remaining; wire[19] = w->remaining>>8; wire[20] = w->global;
    wire[21] = w->paused;
    wire[22] = w->remaining ? w->flags : 0;
    struct Packet p = {0};
    packet_init(&p, PACKET_ROCKET_CAP_STATE, true, PLMT_NONE);
    packet_write(&p, wire, sizeof wire); network_send(&p);
}
static void grant_course_entry(unsigned i) {
    if (gNetworkType != NT_SERVER || !same_area(i) || !alive(&gMarioStates[i]) ||
        (i && !character_net_player_fresh(i))) return;
    const struct NetworkPlayer *np = &gNetworkPlayers[i];
    if (entries[i].seen && entries[i].area == np->currLevelAreaSeqId && entries[i].global == np->globalIndex) return;
    /* These are the same three native entry powers as set_mario_initial_cap_powerup.
     * Require the host's actual loaded level/course, not a peer-supplied cap mask.
     * Level-transition/motion validation remains the base game's responsibility. */
    u32 flag = 0; u16 duration = 0;
    if (gCurrAreaIndex == 1 && gCurrActStarNum != 99) {
        if (gCurrCourseNum == COURSE_COTMC && gCurrLevelNum == LEVEL_COTMC) {
            flag = MARIO_METAL_CAP; duration = gLevelValues.metalCapDurationCotmc;
        } else if (gCurrCourseNum == COURSE_TOTWC && gCurrLevelNum == LEVEL_TOTWC) {
            flag = MARIO_WING_CAP; duration = gLevelValues.wingCapDurationTotwc;
        } else if (gCurrCourseNum == COURSE_VCUTM && gCurrLevelNum == LEVEL_VCUTM) {
            flag = MARIO_VANISH_CAP; duration = gLevelValues.vanishCapDurationVcutm;
        }
    }
    entries[i].seen = 1; entries[i].area = np->currLevelAreaSeqId; entries[i].global = np->globalIndex;
    if (!flag || !duration) return;
    CapLease *w = &leases[i];
    w->flags = flag; w->remaining = duration;
    w->grant = ++nextGrant; if (!w->grant) w->grant = ++nextGrant;
    w->global = np->globalIndex; w->area = np->currLevelAreaSeqId;
    rocket_caps_apply(&gMarioStates[i]);
    if (!i) cap_music(flag);
    send_state(i);
}
static void cancel_local(u8 keep) {
    if (gNetworkType != NT_CLIENT || !gNetworkPlayerServer || !leases[0].remaining) return;
    u64 session = rocket_boost_session_id();
    u8 wire[CANCEL_BYTES] = {0}; put32(wire, session); put32(wire+4, session>>32);
    put32(wire+8, leases[0].grant); wire[12] = leases[0].area; wire[13] = leases[0].area>>8;
    wire[14] = keep & leases[0].flags;
    struct Packet p = {0}; packet_init(&p, PACKET_ROCKET_CAP_CANCEL, true, PLMT_NONE);
    packet_write(&p, wire, sizeof wire); network_send_to(gNetworkPlayerServer->localIndex, &p);
}
int rocket_caps_cancel_allowed(const struct Packet *p) {
    if (!p || p->error || p->dataLength >= PACKET_LENGTH || p->cursor+CANCEL_BYTES != p->dataLength ||
        !online() || gNetworkType != NT_SERVER || !p->localIndex || p->localIndex >= MAX_PLAYERS ||
        !gNetworkPlayers[p->localIndex].connected || p->requestBroadcast || p->orderedGroupId ||
        p->levelAreaMustMatch || p->levelMustMatch ||
        p->destGlobalId != 0) return 0;
    const u8 *b = p->buffer+p->cursor; const CapLease *w = &leases[p->localIndex];
    return current_lease(p->localIndex) && w->remaining && get64(b) == rocket_boost_session_id() &&
        get32(b+8) == w->grant && (b[12] | (u16)b[13]<<8) == w->area &&
        !(b[14] & ~w->flags);
}
void rocket_caps_receive_cancel(struct Packet *p) {
    if (!rocket_caps_cancel_allowed(p)) return;
    unsigned i = p->localIndex;
    leases[i].flags &= p->buffer[p->cursor+14];
    if (!leases[i].flags) { leases[i].remaining = 0; remove_caps(i); }
    else rocket_caps_apply(&gMarioStates[i]);
    send_state(i);
}
int rocket_caps_packet_allowed(const struct Packet *p) {
    if (!p || p->error || p->dataLength >= PACKET_LENGTH || p->cursor + CAP_STATE_BYTES != p->dataLength ||
        !online() || gNetworkType != NT_CLIENT || !gNetworkPlayerServer || !gNetworkPlayerServer->connected ||
        !gNetworkPlayerLocal || p->localIndex != gNetworkPlayerServer->localIndex || p->requestBroadcast ||
        p->orderedGroupId || p->levelMustMatch || p->levelAreaMustMatch ||
        (p->destGlobalId != PACKET_DESTINATION_BROADCAST && p->destGlobalId != gNetworkPlayerLocal->globalIndex)) return 0;
    if (gNetworkServerAddr && (!gNetworkSystem || !gNetworkSystem->match_addr ||
        !gNetworkSystem->match_addr(gNetworkServerAddr, p->addr))) return 0;
    const u8 *b = p->buffer + p->cursor;
    return rocket_boost_session_id() && get64(b) == rocket_boost_session_id() && get32(b+8) &&
        b[20] < MAX_PLAYERS && b[21] <= 1 && !(b[22] & ~MARIO_SPECIAL_CAPS) &&
        ((b[18] || b[19]) ? b[22] && get32(b+12) : !b[22]);
}
void rocket_caps_receive(struct Packet *p) {
    if (!rocket_caps_packet_allowed(p)) return;
    const u8 *b = p->buffer + p->cursor;
    struct NetworkPlayer *np = network_player_from_global_index(b[20]);
    if (!np || np->localIndex >= MAX_PLAYERS) return;
    unsigned i = np->localIndex;
    u16 area = b[16] | (u16)b[17]<<8, remaining = b[18] | (u16)b[19]<<8;
    if (area != np->currLevelAreaSeqId) return;
    /* Joining peers can receive the heartbeat before the avatar/area pose is
     * ready. Do not consume its grant yet; the next heartbeat will retry. */
    if (remaining && (!same_area(i) || !alive(&gMarioStates[i]))) return;
    CapLease *w = &leases[i]; u32 revision = get32(b+8), grant = get32(b+12), delta = revision - w->revision;
    if (w->revision && (!delta || delta >= 0x80000000u)) return;
    /* A delayed heartbeat for the same pickup can only shorten the lease. */
    u8 flags = b[22];
    if (w->grant == grant && w->revision) {
        if (remaining > w->remaining) remaining = w->remaining;
        flags &= w->flags; /* A heartbeat cannot undo a native local removal. */
    }
    if (!flags) remaining = 0;
    int started = remaining && (!w->remaining || w->grant != grant);
    *w = (CapLease){revision, grant, remaining, area, b[20], b[21], flags, clock_elapsed_f64()};
    if (!remaining) remove_caps(i); else rocket_caps_apply(&gMarioStates[i]);
    if (started && i == 0) cap_music(flags);
}
static int contact(const struct MarioState *m, const struct Object *o, float extra) {
    if (!alive(m) || m->action == ACT_GETTING_BLOWN || !o || !(o->activeFlags & ACTIVE_FLAG_ACTIVE) ||
        o->oIntangibleTimer || (o->oInteractStatus & INT_STATUS_INTERACTED) || !m->area ||
        o->header.gfx.activeAreaIndex != m->area->index) return 0;
    float x = m->pos[0]-o->oPosX, y = m->pos[1]-o->oPosY, z = m->pos[2]-o->oPosZ;
    float radius = o->hitboxRadius + 37.f + extra;
    return isfinite(x) && isfinite(y) && isfinite(z) && radius > 0 && radius < 500 &&
        x*x+z*z <= radius*radius && y >= -160.f-extra && y <= o->hitboxHeight+extra;
}
/* Contact kind: -1 unavailable, 0 native Mario, 1 eligible raw car pose.
 * An unavailable selected car must never fall back to its proxy capsule. */
static int pickup_state(unsigned i, struct MarioState *out, RocketSnapshot *pose) {
    if (!same_area(i)) return -1;
    *out = gMarioStates[i];
    if (!alive(out) || out->action == ACT_GETTING_BLOWN) return -1;
    int kind = i ? character_net_pickup_pose(i, pose) : rocket_adapter_pickup_pose(pose);
    if (kind > 0) memcpy(out->pos, pose->position, sizeof out->pos);
    return kind;
}
static int grant(unsigned i, struct Object *o) {
    u32 flag = cap_flag(o->behavior);
    struct MarioState contactState; RocketSnapshot pose;
    if (gNetworkType == NT_CLIENT || !flag || !cap_duration(flag)) return 0;
    int kind = pickup_state(i, &contactState, &pose);
    if (kind < 0) return 0;
    if (kind ? !rocket_adapter_cap_pickup_contact(&pose, o, rocket_caps_active_flags(i)) :
        !contact(&contactState, o, 0)) return 0;
    CapLease *w = &leases[i];
    u16 duration = cap_duration(flag);
    if (i == 0 && gMarioStates[i].capTimer > duration) duration = gMarioStates[i].capTimer;
    if (w->remaining > duration) duration = w->remaining;
    w->flags |= flag;
    w->remaining = duration; w->grant = ++nextGrant; if (!w->grant) w->grant = ++nextGrant;
    w->global = gNetworkPlayers[i].globalIndex; w->area = gNetworkPlayers[i].currLevelAreaSeqId;
    o->oInteractStatus |= INT_STATUS_INTERACTED; /* Reserve before publishing. */
    gMarioStates[i].interactObj = o;
    rocket_caps_apply(&gMarioStates[i]);
    if (i == 0) cap_music(flag);
    network_send_collect_item(o); send_state(i); return 1;
}
int rocket_caps_interact(struct MarioState *m, struct Object *cap) {
    if (!online() || !cap || !cap_flag(cap->behavior)) return 0;
    if (gNetworkType == NT_SERVER && m && m->playerIndex == 0) grant(0, cap);
    return 1; /* Clients await the host; the native item path must not grant. */
}
int rocket_caps_item_allowed(const struct Packet *p) {
    if (!online() || gNetworkType != NT_SERVER) return 1;
    if (!p || p->error || p->dataLength >= PACKET_LENGTH || p->cursor + 16 != p->dataLength) return 0;
    return !cap_flag(get_behavior_from_id(get32(p->buffer+p->cursor)));
}
int rocket_caps_object_allowed(struct Packet *p) {
    if (!online() || gNetworkType != NT_SERVER) return 1;
    if (!p || p->error || p->dataLength >= PACKET_LENGTH || p->cursor + 13 > p->dataLength) return 0;
    const u8 *b = p->buffer+p->cursor;
    struct SyncObject *so = sync_object_get(get32(b+1));
    const BehaviorScript *behavior = get_behavior_from_id(get32(b+9));
    if (cap_flag(behavior) || (so && so->o && cap_flag(so->o->behavior))) return 0;
    if (behavior != bhvExclamationBox && (!so || !so->o || so->o->behavior != bhvExclamationBox)) return 1;
    if (!so || !so->o || so->o->behavior != bhvExclamationBox || behavior != bhvExclamationBox) return 0;
    if (so->o->oBehParams2ndByte > 2) return 1;
    /* Treat a cap-box event as a request. Never apply client object fields or
     * forward it: native host simulation publishes the accepted event. */
    unsigned i = p->localIndex; struct Object *box = so->o;
    struct MarioState contactState; RocketSnapshot pose;
    if (i > 0 && i < MAX_PLAYERS &&
        p->levelAreaMustMatch && p->courseNum == gCurrCourseNum && p->actNum == gCurrActStarNum &&
        p->levelNum == gCurrLevelNum && p->areaIndex == gCurrAreaIndex &&
        (p->destGlobalId == PACKET_DESTINATION_BROADCAST || p->destGlobalId == 0) &&
        b[0] == gNetworkPlayers[i].globalIndex &&
        box->oAction == 2 && !box->oExclamationBoxForce) {
        int kind = pickup_state(i, &contactState, &pose);
        int touched = kind > 0 ? rocket_adapter_cap_box_pose_contact(&pose, box, rocket_caps_active_flags(i)) :
            kind == 0 && contact(&contactState, box, 70) &&
                (determine_interaction(&contactState, box) & INT_ATTACK_NOT_WEAK_FROM_ABOVE);
        if (touched) { box->oExclamationBoxForce = TRUE; network_send_object(box); }
    }
    return 0;
}
void rocket_caps_update(void) {
    if (!online()) { rocket_wing_topper_update(); return; }
    ++heartbeat;
    for (unsigned i = 0; i < MAX_PLAYERS; ++i) {
        grant_course_entry(i);
        CapLease *w = &leases[i]; struct MarioState *m = &gMarioStates[i];
        if (w->remaining) {
            if (!current_lease(i) || !alive(m) || !(m->flags & w->flags) ||
                !(m->flags & (MARIO_CAP_ON_HEAD | MARIO_CAP_IN_HAND)) || (i == 0 && !m->capTimer)) {
                if (i == 0) cancel_local(0);
                w->remaining = 0; remove_caps(i); send_state(i);
            } else {
                u8 keep = m->flags & w->flags;
                if (keep != w->flags) {
                    if (i == 0) cancel_local(keep);
                    w->flags = keep; send_state(i);
                }
                /* Native star exits/cutscenes may shorten the timer. A client
                 * may relinquish time but can never ask for more of it. */
                if (i == 0 && m->capTimer < w->remaining) {
                    cancel_local(0); w->remaining = m->capTimer;
                }
                /* Native pauses belong to the host, never an untrusted client
                 * action. Remote peers cannot freeze expiry with dialog flags. */
                int pause = gNetworkType == NT_SERVER ? i == 0 && native_pause(m, w->remaining) :
                    w->paused && clock_elapsed_f64() - w->received <= 1.0;
                if (gNetworkType == NT_SERVER && w->paused != pause) { w->paused = pause; send_state(i); }
                if (!pause) --w->remaining;
                m->capTimer = w->remaining;
                if (!w->remaining) {
                    remove_caps(i); m->flags &= ~MARIO_SPECIAL_CAPS; m->capTimer = 0;
                    if (!(m->flags & MARIO_CAPS)) m->flags &= ~MARIO_CAP_ON_HEAD;
                    send_state(i);
                }
                else if (w->remaining == 60 && i == 0) fadeout_cap_music();
            }
        }
        if (gNetworkType == NT_SERVER && heartbeat % 15 == 0 && (w->remaining || w->revision)) send_state(i);
    }
    if (gNetworkType == NT_SERVER && gObjectLists) {
        struct ObjectNode *head = &gObjectLists[OBJ_LIST_LEVEL];
        for (struct ObjectNode *node = head->next; node && node != head; node = node->next) {
            struct Object *cap = (struct Object *)node;
            if (!cap_flag(cap->behavior)) continue;
            for (unsigned i = 0; i < MAX_PLAYERS; ++i) if (grant(i, cap)) break;
        }
    }
    rocket_wing_topper_update();
}
