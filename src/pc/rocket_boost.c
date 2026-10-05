/* Host-selected rule. No remote packet ever writes the local preference. */
#include "rocket_boost.h"
#include "configfile.h"
#include "network/network.h"
#include "network/version.h"
#include "utils/misc.h"

#define RULE_BYTES ROCKET_SESSION_RULE_BYTES
#define JOIN_BYTES (MAX_VERSION_LENGTH + 1 + sizeof(s16) + 11 + 512 + RULE_BYTES)
static unsigned sessionMode;
static unsigned sessionSurfaceMode;
static u32 revision;
static u64 session, previousSession;
static double lastSent = -1;

static unsigned preference(void) {
    return configRocketBoostMode == ROCKET_BOOST_INFINITE ? ROCKET_BOOST_INFINITE : ROCKET_BOOST_COIN_ONLY;
}
static unsigned surface_preference(void) {
    return configRocketSurfaceMode == ROCKET_SURFACES_NATIVE ? ROCKET_SURFACES_NATIVE : ROCKET_SURFACES_CAR;
}
int rocket_surface_mode(void) {
    if (gNetworkType == NT_CLIENT) return gCLIOpts.characterNet && revision ? sessionSurfaceMode : ROCKET_SURFACES_CAR;
    return surface_preference();
}
const char *rocket_surface_scope_label(void) {
    if (gNetworkType == NT_CLIENT) return revision ? "Surface behavior is controlled by the host" : "Waiting for host surface rule (Car grip)";
    return "Car grip keeps existing routes. Native surfaces add material sliding and change climbing. Saved for offline play and hosting.";
}
int rocket_boost_mode(void) {
    if (gNetworkType == NT_CLIENT) return gCLIOpts.characterNet && revision ? sessionMode : ROCKET_BOOST_COIN_ONLY;
    return preference();
}
int rocket_boost_can_set_mode(void) { return gNetworkType != NT_CLIENT; }
const char *rocket_boost_mode_label(void) {
    return rocket_boost_mode() == ROCKET_BOOST_INFINITE ? "Infinite" : "Coin only";
}
const char *rocket_boost_scope_label(void) {
    if (gNetworkType == NT_CLIENT) return revision ? "Boost mode is controlled by the host" : "Waiting for host boost rule (coin only)";
    return gNetworkType == NT_SERVER && !gCLIOpts.offline ? "Boost mode applies to everyone in this session" : "Boost mode is saved for offline play and hosting";
}
void rocket_boost_session_reset(void) {
    sessionMode = ROCKET_BOOST_COIN_ONLY;
    sessionSurfaceMode = ROCKET_SURFACES_CAR;
    revision = 0;
    session = 0;
    lastSent = -1;
}
static void refresh_host_rule(void) {
    if (!session) {
        struct timespec now = {0};
        clock_gettime(CLOCK_REALTIME, &now);
        session = (u64)now.tv_sec * 1000000000u + (u64)now.tv_nsec;
        if (session <= previousSession) session = previousSession + 1;
        previousSession = session;
    }
    if (!revision || sessionMode != preference() || sessionSurfaceMode != surface_preference()) {
        sessionMode = preference();
        sessionSurfaceMode = surface_preference();
        if (!++revision) ++revision;
        lastSent = -1;
    }
}
uint64_t rocket_boost_session_id(void) {
    if (gNetworkType == NT_SERVER) refresh_host_rule();
    return session;
}
void rocket_boost_write_rule(struct Packet *p) {
    if (gNetworkType != NT_SERVER) return;
    refresh_host_rule();
    u8 wire[RULE_BYTES] = { sessionMode, revision, revision >> 8, revision >> 16, revision >> 24 };
    for (unsigned i = 0; i < 8; ++i) wire[5 + i] = session >> (8 * i);
    wire[13] = sessionSurfaceMode;
    packet_write(p, wire, sizeof wire);
}
void rocket_boost_network_update(void) {
    if (gNetworkType != NT_SERVER || !gCLIOpts.characterNet || gCLIOpts.offline) return;
    refresh_host_rule();
    double now = clock_elapsed_f64();
    /* Reliable updates plus a heartbeat cover an update racing the join ACK. */
    if (lastSent >= 0 && now >= lastSent && now - lastSent < 1.0) return;
    struct Packet p = {0};
    packet_init(&p, PACKET_ROCKET_BOOST_RULE, true, PLMT_NONE);
    rocket_boost_write_rule(&p);
    network_send(&p);
    lastSent = now;
}
int rocket_boost_set_mode(unsigned mode) {
    if (!rocket_boost_can_set_mode() || mode > ROCKET_BOOST_INFINITE) return 0;
    if (configRocketBoostMode != mode) {
        configRocketBoostMode = mode;
        configfile_save(configfile_name());
    }
    rocket_boost_network_update();
    return 1;
}
int rocket_surface_set_mode(unsigned mode) {
    if (!rocket_boost_can_set_mode() || mode > ROCKET_SURFACES_NATIVE) return 0;
    if (configRocketSurfaceMode != mode) {
        configRocketSurfaceMode = mode;
        configfile_save(configfile_name());
    }
    rocket_boost_network_update();
    return 1;
}
static u32 wire_revision(const u8 *wire) {
    return (u32)wire[1] | (u32)wire[2] << 8 | (u32)wire[3] << 16 | (u32)wire[4] << 24;
}
static u64 wire_session(const u8 *wire) {
    u64 value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (u64)wire[5 + i] << (8 * i);
    return value;
}
static int valid_wire(const u8 *wire) {
    return wire[0] <= ROCKET_BOOST_INFINITE && wire[13] <= ROCKET_SURFACES_NATIVE && wire_revision(wire) && wire_session(wire);
}
static int from_server(const struct Packet *p, int joining) {
    if (!p || p->error || p->dataLength >= PACKET_LENGTH || gNetworkType != NT_CLIENT || !gCLIOpts.characterNet ||
        p->requestBroadcast || p->orderedGroupId || p->levelAreaMustMatch || p->levelMustMatch) return 0;
    if (gNetworkPlayerServer) {
        if (!gNetworkPlayerServer->connected || p->localIndex != gNetworkPlayerServer->localIndex) return 0;
    } else if (!joining || p->localIndex != UNKNOWN_LOCAL_INDEX) return 0;
    /* The join exchange binds this address during mod-list negotiation. */
    if (joining && !gNetworkServerAddr) return 0;
    return !gNetworkServerAddr || (gNetworkSystem && gNetworkSystem->match_addr &&
        gNetworkSystem->match_addr(gNetworkServerAddr, p->addr));
}
int rocket_boost_packet_allowed(const struct Packet *p) {
    return from_server(p, 0) && gNetworkPlayerLocal &&
        (p->destGlobalId == PACKET_DESTINATION_BROADCAST || p->destGlobalId == gNetworkPlayerLocal->globalIndex) &&
        revision && p->cursor + RULE_BYTES == p->dataLength && valid_wire(p->buffer + p->cursor) &&
        session == wire_session(p->buffer + p->cursor);
}
int rocket_boost_join_valid(const struct Packet *p) {
    return from_server(p, 1) && p->cursor + JOIN_BYTES == p->dataLength &&
        valid_wire(p->buffer + p->dataLength - RULE_BYTES);
}
static void read_rule(struct Packet *p) {
    u8 wire[RULE_BYTES];
    packet_read(p, wire, sizeof wire);
    if (p->error || !valid_wire(wire)) return;
    u32 incoming = wire_revision(wire), delta = incoming - revision;
    if (revision && (!delta || delta >= 0x80000000u)) return;
    sessionMode = wire[0];
    sessionSurfaceMode = wire[13];
    revision = incoming;
}
void rocket_boost_read_join(struct Packet *p) {
    /* Called only after join preflight and the exact-version check. */
    session = wire_session(p->buffer + p->cursor);
    revision = 0;
    read_rule(p);
}
void rocket_boost_receive_rule(struct Packet *p) {
    if (rocket_boost_packet_allowed(p)) read_rule(p);
}
