#ifndef SMN64_GRAPHICAL_ALLOCATION_ORDER_H
#define SMN64_GRAPHICAL_ALLOCATION_ORDER_H
#include <stdint.h>

/* Host identities for the source's head-inserted graphical lists. They are
 * neither source ticks nor RNG values. Zero means no graphical allocation.
 * Carry the last allocated serial in the WHOLE pending owner transaction. */
static inline int smn64_graphical_reserve(uint64_t *clock,uint32_t count,uint64_t *base){
    if(!clock||!base||!count||*clock>UINT64_MAX-count)return -1;
    *base=*clock+1u;*clock+=count;return 1;
}
/* Synchronous import only: never retain a pointer in a committed registry.
 * A stale shared clock is an error rather than duplicated live identities. */
static inline int smn64_graphical_import(uint64_t *local,const uint64_t *shared){
    if(!local||!shared||*shared<*local)return -1;
    *local=*shared;return 1;
}

/* Source 61C40 registers these lists; 6731C draws in registration order.
 * This is NOT 6701C's update-list order. In particular F555C lines draw before
 * F5540 polygons, and all F5534 sprites share one cross-owner newest-first list. */
enum SmN64GraphicalList {
    SMN64_GRAPHICAL_SPRITES=0xf5534,
    SMN64_GRAPHICAL_TRAILS=0xf5538,
    SMN64_GRAPHICAL_STRAND_LINES=0xf555c,
    SMN64_GRAPHICAL_POLYGONS=0xf5540,
    SMN64_GRAPHICAL_LINES=0xf5560
};
static inline int smn64_graphical_draw_pass(uint32_t list){
    switch(list){
        case 0xf5534:return 0;case 0xf5538:return 1;case 0xf553c:return 2;
        case 0xf5558:return 3;case 0xf555c:return 4;case 0xf5540:return 5;
        case 0xf5548:return 6;case 0xf5554:return 7;case 0xf554c:return 8;
        case 0xf5550:return 9;case 0xf5560:return 10;default:return -1;
    }
}
/* Renderer merge key: ascending source draw pass, then descending allocation
 * serial. Keys must have a known list and nonzero unique serial. A primitive's
 * endpoints/vertices retain their original order; only primitives are sorted. */
static inline int smn64_graphical_draw_compare(uint32_t a_list,uint64_t a_serial,
                                              uint32_t b_list,uint64_t b_serial){
    int a=smn64_graphical_draw_pass(a_list),b=smn64_graphical_draw_pass(b_list);
    if(a!=b)return a<b?-1:1;
    return a_serial==b_serial?0:(a_serial>b_serial?-1:1);
}
#endif
