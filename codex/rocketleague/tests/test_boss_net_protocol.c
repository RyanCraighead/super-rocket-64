/* Tests the actual authored codec and lease bookkeeping, not native gameplay.
 * Example (from the repository root):
 * cc -std=c11 -Wall -Wextra -Werror -Isrc \
 *   codex/rocketleague/tests/test_boss_net_protocol.c \
 *   src/pc/boss_net_protocol.c -o /tmp/test-boss-net-protocol
 */
#include "pc/boss_net_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static BossNetMessage sample_message(void) {
    BossNetMessage m = {0};
    m.key.sync = 0x345;
    m.key.level = 30;
    m.key.course = 15;
    m.key.act = 6;
    m.key.area = 3;
    m.key.kind = 2;
    m.epoch = UINT32_C(0x12345678);
    m.revision = UINT32_C(0x89abcdef);
    m.token = 0xb2a1;
    m.length = 4;
    m.op = BN_PASS;
    m.owner = 7;
    return m;
}

static void same_lease(const BossNetLease *a, const BossNetLease *b) {
    CHECK(a->owner == b->owner && a->epoch == b->epoch &&
          a->revision == b->revision && a->event == b->event && a->events == b->events);
}

static void reject_wire(const uint8_t *wire, size_t length) {
    BossNetMessage output, before;
    memset(&output, 0xa5, sizeof(output));
    memcpy(&before, &output, sizeof(before));
    CHECK(!boss_net_unpack(&output, wire, length));
    CHECK(!memcmp(&output, &before, sizeof(output)));
}

static void test_encoding(void) {
    static const uint8_t body[] = {0x00, 0xaa, 0xff, 0x13};
    static const uint8_t golden[] = {
        1, 3, 2, 7, 15, 6, 3, 0, 30, 0, 0xa1, 0xb2,
        0x45, 0x03, 0, 0, 0x78, 0x56, 0x34, 0x12,
        0xef, 0xcd, 0xab, 0x89, 4, 0, 0, 0, 0x00, 0xaa, 0xff, 0x13
    };
    uint8_t wire[BOSS_NET_HEADER + BOSS_NET_BODY + 1];
    BossNetMessage m = sample_message(), decoded;
    CHECK(BOSS_NET_HEADER == 28);
    memset(wire, 0xcc, sizeof(wire));
    CHECK(boss_net_pack(wire, sizeof(golden), &m, body) == (int)sizeof(golden));
    CHECK(!memcmp(wire, golden, sizeof(golden)));
    CHECK(wire[sizeof(golden)] == 0xcc);
    CHECK(boss_net_unpack(&decoded, golden, sizeof(golden)));
    CHECK(boss_net_key_equal(&decoded.key, &m.key));
    CHECK(decoded.epoch == m.epoch && decoded.revision == m.revision &&
          decoded.token == m.token && decoded.owner == m.owner &&
          decoded.op == m.op && decoded.length == m.length);
    for (size_t n = 0; n < sizeof(golden); ++n) reject_wire(golden, n);
    reject_wire(wire, sizeof(golden) + 1);
    reject_wire(NULL, sizeof(golden));
    CHECK(!boss_net_unpack(NULL, golden, sizeof(golden)));

    /* A rejected pack must not partially replace an existing packet. */
    memset(wire, 0xcc, sizeof(wire));
    CHECK(!boss_net_pack(wire, sizeof(golden) - 1, &m, body));
    CHECK(!boss_net_pack(wire, sizeof(wire), &m, NULL));
    CHECK(!boss_net_pack(wire, sizeof(wire), NULL, body));
    CHECK(!boss_net_pack(NULL, sizeof(wire), &m, body));
    for (size_t n = 0; n < sizeof(wire); ++n) CHECK(wire[n] == 0xcc);

    struct Mutation { unsigned offset; uint8_t value; } mutations[] = {
        {0, 0}, {0, 2}, {1, 0}, {1, BN_REWARD + 1}, {2, 0}, {2, 5},
        {6, 0}, {6, 9}, {7, 1}, {8, 0}, {8, 64}, {9, 1},
        {24, 3}, {24, 5}, {25, 0xff}, {26, 1}, {27, 1}
    };
    for (size_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        memcpy(wire, golden, sizeof(golden));
        wire[mutations[i].offset] = mutations[i].value;
        reject_wire(wire, sizeof(golden));
    }
    memcpy(wire, golden, sizeof(golden));
    memset(wire + 12, 0, 4); /* Missing static sync ID. */
    reject_wire(wire, sizeof(golden));
    wire[13] = 0x10; /* First non-static sync ID: 4096. */
    reject_wire(wire, sizeof(golden));
    memcpy(wire, golden, sizeof(golden));
    memset(wire + 16, 0, 4); /* Non-JOIN messages need an epoch. */
    reject_wire(wire, sizeof(golden));

    m.length = 0;
    for (unsigned op = BN_JOIN; op <= BN_REWARD; ++op) {
        m.op = (uint8_t)op;
        CHECK(boss_net_pack(wire, sizeof(wire), &m, NULL) == BOSS_NET_HEADER);
        CHECK(boss_net_unpack(&decoded, wire, BOSS_NET_HEADER));
        CHECK(decoded.op == op);
    }
    m.op = BN_JOIN;
    m.epoch = 0;
    m.owner = BOSS_NET_NO_OWNER;
    CHECK(boss_net_pack(wire, sizeof(wire), &m, NULL) == BOSS_NET_HEADER);
    CHECK(boss_net_unpack(&decoded, wire, BOSS_NET_HEADER));
    m.length = 1;
    CHECK(!boss_net_pack(wire, sizeof(wire), &m, body));
    m = sample_message();
    m.length = BOSS_NET_BODY + 1;
    CHECK(!boss_net_pack(wire, sizeof(wire), &m, wire));

    uint8_t maximum[BOSS_NET_BODY];
    for (size_t i = 0; i < sizeof(maximum); ++i) maximum[i] = (uint8_t)(i % 251);
    m.length = BOSS_NET_BODY;
    CHECK(boss_net_pack(wire, sizeof(wire) - 1, &m, maximum) == BOSS_NET_HEADER + BOSS_NET_BODY);
    CHECK(boss_net_unpack(&decoded, wire, sizeof(wire) - 1));
    CHECK(decoded.length == BOSS_NET_BODY);
    CHECK(!memcmp(wire + BOSS_NET_HEADER, maximum, sizeof(maximum)));
    wire[24] = (uint8_t)(BOSS_NET_BODY + 1);
    wire[25] = (uint8_t)((BOSS_NET_BODY + 1) >> 8);
    reject_wire(wire, sizeof(wire));
}

static void test_key_and_serials(void) {
    BossNetKey a = sample_message().key, b = a;
    CHECK(boss_net_key_equal(&a, &b));
#define DIFFERENT(field) do { b = a; ++b.field; CHECK(!boss_net_key_equal(&a, &b)); } while (0)
    DIFFERENT(sync); DIFFERENT(level); DIFFERENT(course);
    DIFFERENT(act); DIFFERENT(area); DIFFERENT(kind);
#undef DIFFERENT
    CHECK(!boss_net_newer(7, 7));
    CHECK(boss_net_newer(8, 7));
    CHECK(!boss_net_newer(7, 8));
    CHECK(boss_net_newer(0, UINT32_MAX));
    CHECK(!boss_net_newer(UINT32_MAX, 0));
    CHECK(boss_net_newer(UINT32_C(0x7fffffff), 0));
    CHECK(!boss_net_newer(UINT32_C(0x80000000), 0));
    CHECK(!boss_net_newer(0, UINT32_C(0x80000000)));
}

static void test_grants_and_states(void) {
    BossNetLease l = {0}, before;
    CHECK(!boss_net_lease_grant(NULL, 2, 5));
    CHECK(!boss_net_lease_grant(&l, 2, 0));
    CHECK(boss_net_lease_grant(&l, 2, 5));
    CHECK(l.owner == 2 && l.epoch == 5 && !l.revision && !l.event && !l.events);
    CHECK(boss_net_lease_state(&l, 2, 5, 10));
    CHECK(boss_net_lease_event(&l, 2, 5, 1));
    CHECK(boss_net_lease_event(&l, 2, 5, 3));
    before = l;
    CHECK(boss_net_lease_grant(&l, 2, 5)); /* Retransmission, not reset. */
    same_lease(&l, &before);
    CHECK(!boss_net_lease_grant(&l, 3, 5));
    same_lease(&l, &before);
    CHECK(!boss_net_lease_grant(&l, 2, 4));
    same_lease(&l, &before);
    CHECK(!boss_net_lease_grant(&l, 2, UINT32_C(0x80000005)));
    same_lease(&l, &before);
    CHECK(!boss_net_lease_state(NULL, 2, 5, 11));
    CHECK(!boss_net_lease_state(&l, 3, 5, 11));
    CHECK(!boss_net_lease_state(&l, BOSS_NET_NO_OWNER, 5, 11));
    CHECK(!boss_net_lease_state(&l, 2, 0, 11));
    CHECK(!boss_net_lease_state(&l, 2, 4, 11));
    CHECK(!boss_net_lease_state(&l, 2, 6, 11));
    CHECK(!boss_net_lease_state(&l, 2, 5, 10));
    CHECK(!boss_net_lease_state(&l, 2, 5, 9));
    same_lease(&l, &before);
    CHECK(!boss_net_lease_event(&l, 2, 5, 1));
    CHECK(!boss_net_lease_event(&l, 2, 5, 3));
    same_lease(&l, &before);
    CHECK(boss_net_lease_state(&l, 2, 5, 11));
    CHECK(l.event == before.event && l.events == before.events);

    CHECK(boss_net_lease_grant(&l, 3, 6));
    CHECK(l.owner == 3 && l.epoch == 6 && !l.revision && !l.event && !l.events);
    before = l;
    CHECK(!boss_net_lease_state(&l, 2, 5, 12));
    CHECK(!boss_net_lease_state(&l, 3, 5, 12));
    CHECK(!boss_net_lease_event(&l, 2, 5, 2));
    CHECK(!boss_net_lease_event(&l, 3, 5, 2));
    same_lease(&l, &before);
    CHECK(boss_net_lease_state(&l, 3, 6, 1));
    CHECK(boss_net_lease_event(&l, 3, 6, 1));

    l = (BossNetLease){.owner = 3, .epoch = UINT32_MAX, .revision = UINT32_MAX};
    CHECK(boss_net_lease_state(&l, 3, UINT32_MAX, 0));
    CHECK(boss_net_lease_state(&l, 3, UINT32_MAX, 1));
    CHECK(!boss_net_lease_state(&l, 3, UINT32_MAX, UINT32_MAX));
    CHECK(boss_net_lease_grant(&l, 4, 1)); /* Epoch rollover skips reserved zero. */
    CHECK(l.epoch == 1 && l.owner == 4 && !l.revision);
    CHECK(!boss_net_lease_grant(&l, 3, UINT32_MAX));
    CHECK(boss_net_lease_grant(&l, BOSS_NET_NO_OWNER, 2));
    CHECK(!boss_net_lease_state(&l, BOSS_NET_NO_OWNER, 2, 1));
    CHECK(!boss_net_lease_event(&l, BOSS_NET_NO_OWNER, 2, 1));
}

static void test_reliable_event_barrier(void) {
    BossNetLease l = {0}, before;
    CHECK(boss_net_lease_grant(&l, 2, 5));
    before = l;
    CHECK(!boss_net_lease_event(NULL, 2, 5, 1));
    CHECK(!boss_net_lease_event(&l, 3, 5, 1));
    CHECK(!boss_net_lease_event(&l, 2, 0, 1));
    CHECK(!boss_net_lease_event(&l, 2, 4, 1));
    CHECK(!boss_net_lease_event(&l, 2, 6, 1));
    CHECK(!boss_net_lease_event(&l, 2, 5, 0));
    CHECK(!boss_net_lease_event(&l, 2, 5, UINT32_MAX));
    CHECK(!boss_net_lease_event(&l, 2, 5, 65));
    same_lease(&l, &before);

    /* PASS declaring final event 3 must wait even if its final state already
     * arrived. These are the exposed barrier prerequisites, not a substitute
     * for testing the integration's PASS handler or its effect dispatch. */
    CHECK(boss_net_lease_state(&l, 2, 5, 20));
    CHECK(boss_net_lease_event(&l, 2, 5, 3));
    CHECK(l.event < 3 && l.events != 0);
    CHECK(boss_net_lease_event(&l, 2, 5, 1));
    CHECK(l.event == 1 && l.event < 3 && l.events != 0);
    before = l;
    CHECK(!boss_net_lease_event(&l, 2, 5, 3));
    same_lease(&l, &before);
    CHECK(boss_net_lease_grant(&l, 2, 5));
    same_lease(&l, &before);
    CHECK(boss_net_lease_event(&l, 2, 5, 2));
    CHECK(l.event == 3 && l.events == 0 && l.revision == 20);
    CHECK(!boss_net_lease_event(&l, 2, 5, 2));
    CHECK(!boss_net_lease_event(&l, 2, 5, 3));
    CHECK(boss_net_lease_grant(&l, 4, 6));
    CHECK(!boss_net_lease_event(&l, 2, 5, 4));
    CHECK(!boss_net_lease_event(&l, 4, 5, 4));

    /* A missing event must survive a full window of later deliveries. The
     * rejected event 65 may be retried after closing that gap. */
    CHECK(boss_net_lease_event(&l, 4, 6, 64));
    CHECK(l.event == 0 && l.events == (UINT64_C(1) << 63));
    for (uint32_t event = 63; event >= 2; --event) {
        CHECK(boss_net_lease_event(&l, 4, 6, event));
        CHECK(l.event == 0);
    }
    CHECK(l.events == UINT64_MAX - 1);
    before = l;
    CHECK(!boss_net_lease_event(&l, 4, 6, 65));
    CHECK(!boss_net_lease_event(&l, 4, 6, 64));
    same_lease(&l, &before);
    CHECK(boss_net_lease_event(&l, 4, 6, 1));
    CHECK(l.event == 64 && l.events == 0);
    CHECK(boss_net_lease_event(&l, 4, 6, 65));
    CHECK(l.event == 65 && l.events == 0);
    CHECK(!boss_net_lease_event(&l, 4, 6, 1));
    CHECK(!boss_net_lease_event(&l, 4, 6, 64));

    /* A claimed PASS barrier behind an already observed future event is not
     * a drained handoff, even while the contiguous value happens to match. */
    CHECK(boss_net_lease_event(&l, 4, 6, 67));
    CHECK(l.event == 65 && l.events != 0);
    CHECK(boss_net_lease_event(&l, 4, 6, 66));
    CHECK(l.event == 67 && l.events == 0);
}

static void test_dynamic_event_ledger(void) {
    BossNetLedger ledger = {0}, other = {0};
    CHECK(!boss_net_ledger_accept(NULL, 1));
    CHECK(!boss_net_ledger_accept(&ledger, 0));
    CHECK(!boss_net_ledger_accept(&ledger, UINT32_MAX));
    CHECK(!ledger.bits && !ledger.capacity && !ledger.highest);

    /* More than 64 reliable messages may beat the first one. None may erase
     * that hole or become acceptable twice merely because newer ones arrived. */
    for (uint32_t sequence = 1024; sequence >= 2; --sequence)
        CHECK(boss_net_ledger_accept(&ledger, sequence));
    CHECK(ledger.contiguous == 0 && ledger.highest == 1024);
    CHECK(!boss_net_ledger_accept(&ledger, 2));
    CHECK(!boss_net_ledger_accept(&ledger, 65));
    CHECK(!boss_net_ledger_accept(&ledger, 1024));
    CHECK(boss_net_ledger_accept(&ledger, 1));
    CHECK(ledger.contiguous == 1024 && ledger.highest == 1024);
    CHECK(!boss_net_ledger_accept(&ledger, 1));

    /* An independent channel's initial baseline declares only that prefix
     * delivered. Resetting either channel must not affect the other. */
    boss_net_ledger_reset(&other, 100);
    CHECK(other.base == 100 && other.contiguous == 100 && other.highest == 100);
    CHECK(!other.bits && !other.capacity);
    CHECK(!boss_net_ledger_accept(&other, 99));
    CHECK(!boss_net_ledger_accept(&other, 100));
    CHECK(boss_net_ledger_accept(&other, 104));
    CHECK(boss_net_ledger_accept(&other, 103));
    CHECK(boss_net_ledger_accept(&other, 101));
    CHECK(other.contiguous == 101 && other.highest == 104);
    boss_net_ledger_reset(&ledger, 0);
    CHECK(!ledger.bits && !ledger.capacity && !ledger.base &&
          !ledger.contiguous && !ledger.highest);
    CHECK(!boss_net_ledger_accept(&other, 103));
    CHECK(boss_net_ledger_accept(&other, 102));
    CHECK(other.contiguous == 104 && other.highest == 104);

    /* Force two acknowledged-prefix compactions with both near and far
     * future messages retained. Compaction must preserve every pending bit. */
    CHECK(boss_net_ledger_accept(&ledger, 17000));
    CHECK(boss_net_ledger_accept(&ledger, 8194));
    for (uint32_t sequence = 1; sequence < 8192; ++sequence)
        CHECK(boss_net_ledger_accept(&ledger, sequence));
    CHECK(ledger.contiguous == 8191 && ledger.highest == 17000);
    CHECK(boss_net_ledger_accept(&ledger, 8192));
    CHECK(ledger.base >= 8192 && ledger.base % 8 == 0);
    CHECK(ledger.contiguous == 8192 && ledger.highest == 17000);
    CHECK(!boss_net_ledger_accept(&ledger, 8194));
    CHECK(!boss_net_ledger_accept(&ledger, 17000));
    CHECK(boss_net_ledger_accept(&ledger, 8193));
    CHECK(ledger.contiguous == 8194);
    for (uint32_t sequence = 8195; sequence < 17000; ++sequence)
        CHECK(boss_net_ledger_accept(&ledger, sequence));
    CHECK(ledger.base >= 16384 && ledger.base % 8 == 0);
    CHECK(ledger.contiguous == 17000 && ledger.highest == 17000);
    CHECK(!boss_net_ledger_accept(&ledger, 1));
    CHECK(!boss_net_ledger_accept(&ledger, 8193));
    CHECK(!boss_net_ledger_accept(&ledger, 16385));
    CHECK(!boss_net_ledger_accept(&ledger, 17000));
    CHECK(boss_net_ledger_accept(&ledger, 17001));
    CHECK(ledger.contiguous == 17001 && ledger.highest == 17001);

    /* Bound malformed-gap allocation without turning it into a receipt.
     * The inclusive maximum still retains the missing first event. */
    boss_net_ledger_reset(&ledger, 0);
    CHECK(!boss_net_ledger_accept(&ledger, 1048577));
    CHECK(!ledger.bits && !ledger.capacity && !ledger.highest);
    CHECK(boss_net_ledger_accept(&ledger, 1048576));
    CHECK(ledger.capacity <= 131072 && ledger.contiguous == 0 && ledger.highest == 1048576);
    CHECK(!boss_net_ledger_accept(&ledger, 1048576));
    CHECK(!boss_net_ledger_accept(&ledger, 1048577));
    CHECK(!boss_net_ledger_accept(&ledger, UINT32_MAX));
    CHECK(ledger.contiguous == 0 && ledger.highest == 1048576);
    CHECK(boss_net_ledger_accept(&ledger, 1));
    CHECK(ledger.contiguous == 1 && ledger.highest == 1048576);

    /* A high initial baseline must not overflow the gap calculation. Zero is
     * reserved; this ledger does not promise sequence rollover within a run. */
    boss_net_ledger_reset(&ledger, UINT32_MAX - 3);
    CHECK(!ledger.bits && !ledger.capacity);
    CHECK(!boss_net_ledger_accept(&ledger, UINT32_MAX - 3));
    CHECK(boss_net_ledger_accept(&ledger, UINT32_MAX - 1));
    CHECK(ledger.contiguous == UINT32_MAX - 3);
    CHECK(boss_net_ledger_accept(&ledger, UINT32_MAX - 2));
    CHECK(ledger.contiguous == UINT32_MAX - 1);
    CHECK(boss_net_ledger_accept(&ledger, UINT32_MAX));
    CHECK(ledger.contiguous == UINT32_MAX && ledger.highest == UINT32_MAX);
    CHECK(!boss_net_ledger_accept(&ledger, 0));
    CHECK(!boss_net_ledger_accept(&ledger, UINT32_MAX));

    boss_net_ledger_reset(&ledger, 0);
    boss_net_ledger_reset(&ledger, 0); /* Repeated cleanup is safe. */
    boss_net_ledger_reset(&other, 0);
    CHECK(!ledger.bits && !ledger.capacity && !ledger.base && !ledger.contiguous && !ledger.highest);
    CHECK(!other.bits && !other.capacity && !other.base && !other.contiguous && !other.highest);
}

static void test_late_delivery_baseline(void) {
    BossNetLedger ledger = {0};
    boss_net_ledger_baseline(NULL, 100);
    boss_net_ledger_baseline(&ledger, 100);
    CHECK(ledger.base == 100 && ledger.contiguous == 100 && ledger.highest == 100);
    CHECK(!ledger.bits && !ledger.capacity);
    CHECK(!boss_net_ledger_accept(&ledger, 100));
    CHECK(boss_net_ledger_accept(&ledger, 101));
    boss_net_ledger_reset(&ledger, 0);

    /* Effects may beat their initial GRANT. Admitting its prefix must preserve
     * later receipts and holes, without applying those effects a second time. */
    CHECK(boss_net_ledger_accept(&ledger, 1028));
    CHECK(boss_net_ledger_accept(&ledger, 1026));
    boss_net_ledger_baseline(&ledger, 1024);
    CHECK(ledger.base == 1024 && ledger.contiguous == 1024 && ledger.highest == 1028);
    CHECK(!boss_net_ledger_accept(&ledger, 1026));
    CHECK(!boss_net_ledger_accept(&ledger, 1028));
    boss_net_ledger_baseline(&ledger, 1000); /* A delayed older baseline cannot rewind. */
    boss_net_ledger_baseline(&ledger, 1024); /* Duplicate GRANT is idempotent. */
    CHECK(ledger.contiguous == 1024 && ledger.highest == 1028);
    CHECK(boss_net_ledger_accept(&ledger, 1025));
    CHECK(ledger.contiguous == 1026 && ledger.highest == 1028);
    boss_net_ledger_baseline(&ledger, 1027);
    CHECK(ledger.contiguous == 1028 && ledger.highest == 1028);
    CHECK(!boss_net_ledger_accept(&ledger, 1028));
    boss_net_ledger_reset(&ledger, 0);

    /* Compare an independently recorded receipt set across byte alignments,
     * including baselines below an existing contiguous prefix and beyond all
     * observed messages. The oracle does not reuse ledger bit operations. */
    for (uint32_t baseline = 1; baseline <= 100; ++baseline) {
        unsigned char seen[97] = {0};
        for (uint32_t sequence = 96; sequence > 0; --sequence) {
            if (sequence <= 5 || sequence % 3 == 0 || sequence == 65) {
                CHECK(boss_net_ledger_accept(&ledger, sequence));
                seen[sequence] = 1;
            }
        }
        boss_net_ledger_baseline(&ledger, baseline);
        uint32_t expected = baseline;
        while (expected < 96 && seen[expected + 1]) ++expected;
        CHECK(ledger.contiguous == expected);
        CHECK(ledger.highest == (baseline > 96 ? baseline : 96));
        for (uint32_t sequence = 1; sequence <= 96; ++sequence) {
            int should_accept = sequence > baseline && !seen[sequence];
            CHECK(boss_net_ledger_accept(&ledger, sequence) == should_accept);
        }
        CHECK(ledger.contiguous == (baseline > 96 ? baseline : 96));
        CHECK(ledger.highest == ledger.contiguous);
        boss_net_ledger_reset(&ledger, 0);
    }

    /* A previously compacted ledger has a nonzero bit origin. A new baseline
     * may move that origin again while retaining a receipt far above it. */
    boss_net_ledger_reset(&ledger, 700000);
    CHECK(boss_net_ledger_accept(&ledger, 710000));
    for (uint32_t sequence = 700001; sequence <= 708200; ++sequence)
        CHECK(boss_net_ledger_accept(&ledger, sequence));
    CHECK(ledger.base > 700000 && ledger.contiguous == 708200);
    boss_net_ledger_baseline(&ledger, 708203);
    CHECK(ledger.contiguous == 708203 && ledger.highest == 710000);
    CHECK(!boss_net_ledger_accept(&ledger, 710000));
    CHECK(boss_net_ledger_accept(&ledger, 708204));
    boss_net_ledger_baseline(&ledger, 709999);
    CHECK(ledger.contiguous == 710000 && ledger.highest == 710000);
    CHECK(!boss_net_ledger_accept(&ledger, 710000));

    /* A far-ahead admitted baseline is not a sparse allocation request. */
    boss_net_ledger_baseline(&ledger, UINT32_MAX - 2);
    CHECK(!ledger.bits && !ledger.capacity);
    CHECK(ledger.contiguous == UINT32_MAX - 2 && ledger.highest == UINT32_MAX - 2);
    CHECK(boss_net_ledger_accept(&ledger, UINT32_MAX));
    boss_net_ledger_baseline(&ledger, UINT32_MAX - 1);
    CHECK(ledger.contiguous == UINT32_MAX && ledger.highest == UINT32_MAX);
    CHECK(!boss_net_ledger_accept(&ledger, UINT32_MAX));
    boss_net_ledger_baseline(&ledger, UINT32_MAX);
    CHECK(!ledger.bits && !ledger.capacity && ledger.base == UINT32_MAX);
    boss_net_ledger_reset(&ledger, 0);
}

int main(void) {
    test_encoding();
    test_key_and_serials();
    test_grants_and_states();
    test_reliable_event_barrier();
    test_dynamic_event_ledger();
    test_late_delivery_baseline();
    printf("boss network protocol: %u checks passed (codec/leases/ledgers/barriers; no gameplay)\n", checks);
    return 0;
}
