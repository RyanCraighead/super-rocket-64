/* Authored crossover protocol. No native structs or pointers enter the wire. */
#include "boss_net_protocol.h"
#include <string.h>
#include <stdlib.h>
static uint32_t read32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void write32(uint8_t *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static int valid(const BossNetMessage *m){
    return m&&m->op>=BN_JOIN&&m->op<=BN_REWARD&&m->key.kind>=1&&m->key.kind<=4&&
        m->key.level>0&&m->key.level<64&&m->key.area>0&&m->key.area<=8&&m->key.sync>0&&
        m->key.sync<4096&&m->length<=BOSS_NET_BODY&&
        (m->op==BN_JOIN?(m->length==0):m->epoch!=0);
}
int boss_net_pack(uint8_t *w,size_t n,const BossNetMessage *m,const void *body){
    if(!w||!valid(m)||n<(size_t)BOSS_NET_HEADER+m->length||(m->length&&!body))return 0;
    memset(w,0,BOSS_NET_HEADER);w[0]=1;w[1]=m->op;w[2]=m->key.kind;w[3]=m->owner;
    w[4]=m->key.course;w[5]=m->key.act;w[6]=m->key.area;w[8]=(uint8_t)m->key.level;w[9]=m->key.level>>8;
    w[10]=(uint8_t)m->token;w[11]=m->token>>8;write32(w+12,m->key.sync);write32(w+16,m->epoch);write32(w+20,m->revision);
    w[24]=(uint8_t)m->length;w[25]=m->length>>8;
    if(m->length)memcpy(w+BOSS_NET_HEADER,body,m->length);
    return BOSS_NET_HEADER+m->length;
}
int boss_net_unpack(BossNetMessage *m,const uint8_t *w,size_t n){
    if(!m||!w||n<BOSS_NET_HEADER||w[0]!=1||w[7]||w[26]||w[27])return 0;
    BossNetMessage v={0};v.op=w[1];v.key.kind=w[2];v.owner=w[3];v.key.course=w[4];v.key.act=w[5];v.key.area=w[6];
    v.key.level=w[8]|((uint16_t)w[9]<<8);v.token=w[10]|((uint16_t)w[11]<<8);
    v.key.sync=read32(w+12);v.epoch=read32(w+16);v.revision=read32(w+20);v.length=w[24]|((uint16_t)w[25]<<8);
    if(n!=(size_t)BOSS_NET_HEADER+v.length||!valid(&v))return 0;
    *m=v;return 1;
}
int boss_net_key_equal(const BossNetKey *a,const BossNetKey *b){
    return a->sync==b->sync&&a->level==b->level&&a->course==b->course&&a->act==b->act&&a->area==b->area&&a->kind==b->kind;
}
int boss_net_newer(uint32_t n,uint32_t p){return n!=p&&(uint32_t)(n-p)<0x80000000u;}
int boss_net_lease_state(BossNetLease *l,uint8_t sender,uint32_t epoch,uint32_t rev){
    if(!l||sender==BOSS_NET_NO_OWNER||sender!=l->owner||!epoch||epoch!=l->epoch||!boss_net_newer(rev,l->revision))return 0;
    l->revision=rev;return 1;
}
int boss_net_lease_event(BossNetLease *l,uint8_t sender,uint32_t epoch,uint32_t event){
    if(!l||sender==BOSS_NET_NO_OWNER||sender!=l->owner||!epoch||epoch!=l->epoch||!event)return 0;
    /* event is the contiguous delivery barrier. The mask admits the next 64
     * reliable events out of order without forgetting a missing older event. */
    uint32_t d=event-l->event;
    if(!d||d>64||(l->events&(UINT64_C(1)<<(d-1))))return 0;
    l->events|=UINT64_C(1)<<(d-1);
    while(l->events&1){l->event++;l->events>>=1;}
    return 1;
}
int boss_net_lease_grant(BossNetLease *l,uint8_t owner,uint32_t epoch){
    if(!l||!epoch)return 0;
    if(l->epoch==epoch)return l->owner==owner;
    if(l->epoch&&!boss_net_newer(epoch,l->epoch))return 0;
    memset(l,0,sizeof(*l));l->owner=owner;l->epoch=epoch;return 1;
}
void boss_net_ledger_reset(BossNetLedger *l,uint32_t baseline){
    free(l->bits);memset(l,0,sizeof(*l));l->base=l->contiguous=l->highest=baseline;
}
int boss_net_ledger_accept(BossNetLedger *l,uint32_t sequence){
    if(!l||sequence<=l->base)return 0;
    uint32_t delta=sequence-l->base;
    /* A single million-event gap is malformed, not ordinary UDP reordering.
     * Bound allocation to 128 KiB per active channel, independently of uptime. */
    if(delta>1048576)return 0;
    uint32_t byte=(delta-1)/8;uint8_t bit=1u<<((delta-1)%8);
    if(byte>=l->capacity){
        uint32_t capacity=l->capacity?l->capacity:32;
        while(capacity<=byte)capacity*=2;
        uint8_t *bits=realloc(l->bits,capacity);if(!bits)return 0;
        memset(bits+l->capacity,0,capacity-l->capacity);l->bits=bits;l->capacity=capacity;
    }
    if(l->bits[byte]&bit)return 0;
    l->bits[byte]|=bit;if(sequence>l->highest)l->highest=sequence;
    while(l->contiguous<l->highest){
        uint32_t at=l->contiguous-l->base;
        if(!(l->bits[at/8]&(1u<<(at%8))))break;
        l->contiguous++;
    }
    uint32_t consumed=(l->contiguous-l->base)/8;
    if(consumed>=1024){
        memmove(l->bits,l->bits+consumed,l->capacity-consumed);
        memset(l->bits+l->capacity-consumed,0,consumed);l->base+=consumed*8;
    }
    return 1;
}
void boss_net_ledger_baseline(BossNetLedger *l,uint32_t baseline){
    if(!l||baseline<=l->base)return;
    BossNetLedger previous=*l;memset(l,0,sizeof(*l));
    l->base=l->contiguous=l->highest=baseline;
    for(uint32_t at=baseline;at<previous.highest;at++){
        uint32_t bit=at-previous.base;
        if(previous.bits[bit/8]&(1u<<(bit%8)))boss_net_ledger_accept(l,at+1);
    }
    free(previous.bits);
}
