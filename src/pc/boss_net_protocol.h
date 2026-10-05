#ifndef BOSS_NET_PROTOCOL_H
#define BOSS_NET_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define BOSS_NET_HEADER 28
#define BOSS_NET_BODY 2600
#define BOSS_NET_NO_OWNER 255
enum BossNetOp { BN_JOIN=1, BN_STATE, BN_PASS, BN_GRANT, BN_EFFECT, BN_REWARD };
typedef struct BossNetKey {
    uint32_t sync;
    uint16_t level;
    uint8_t course,act,area,kind;
} BossNetKey;
typedef struct BossNetMessage {
    BossNetKey key;
    uint32_t epoch,revision;
    uint16_t token,length;
    uint8_t op,owner;
} BossNetMessage;
typedef struct BossNetLease {
    uint32_t epoch,revision,event;
    uint64_t events;
    uint8_t owner;
} BossNetLease;
/* Reliable event ledger retains holes even when more than 64 packets arrive
 * out of order. Fully acknowledged prefixes are compacted. */
typedef struct BossNetLedger {
    uint8_t *bits;
    uint32_t base,contiguous,highest,capacity;
} BossNetLedger;
void boss_net_ledger_reset(BossNetLedger *ledger,uint32_t baseline);
void boss_net_ledger_baseline(BossNetLedger *ledger,uint32_t baseline);
int boss_net_ledger_accept(BossNetLedger *ledger,uint32_t sequence);
int boss_net_pack(uint8_t *wire,size_t capacity,const BossNetMessage *message,const void *body);
int boss_net_unpack(BossNetMessage *message,const uint8_t *wire,size_t length);
int boss_net_key_equal(const BossNetKey *a,const BossNetKey *b);
int boss_net_newer(uint32_t next,uint32_t previous);
int boss_net_lease_state(BossNetLease *lease,uint8_t sender,uint32_t epoch,uint32_t revision);
int boss_net_lease_event(BossNetLease *lease,uint8_t sender,uint32_t epoch,uint32_t event);
int boss_net_lease_grant(BossNetLease *lease,uint8_t owner,uint32_t epoch);
#endif
