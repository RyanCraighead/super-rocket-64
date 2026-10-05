#include "air_interrupt_n64.h"
#include <string.h>
static int phase(const SmN64AirInterruptHost *h,SmN64AirAttack *s,uint32_t p) {
    return h&&h->phase&&h->phase(h->context,s,p)==1?1:-2;
}
int smn64_air_attack_interrupt(SmN64AirAttack *s,uint32_t prior,uint32_t damaged,
    uint32_t now,uint32_t first,uint32_t *jump,uint16_t *speed,
    const uint16_t *counts,size_t n,const SmN64AirInterruptHost *h) {
    if(!s||!jump||!speed||s->state!=0x1000000u)return -1;
    *speed=0;
    if(s->landing_wait||!(prior&0x1000000u)||(uint32_t)(now-damaged)>=6u)return 0;
    if(!counts||n<=175||!counts[175]||counts[175]>INT16_MAX)return -1;
    if((s->anim.animation==129||s->anim.animation==133)&&s->anim.finished&&!first)
        if(phase(h,s,SMN64_AIR_INTERRUPT_START_TRAILS)!=1)return -2;
    s->normal[0]=0;s->normal[1]=-4096;s->normal[2]=0;
    if(phase(h,s,SMN64_AIR_INTERRUPT_ALIGN_FLAT)!=1)return -2;
    smn64_anim_run(&s->anim,175,counts[175],0,-1);
    if(phase(h,s,SMN64_AIR_INTERRUPT_STOP_TRAILS)!=1)return -2;
    memset(s->velocity,0,sizeof(s->velocity));*jump=0;s->state=4;
    return 1;
}
