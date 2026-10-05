#include "trails_n64.h"
#include <string.h>

int smn64_trail_init(SmN64Trail *s, const int32_t p[3], uint32_t color) {
    unsigned i,j;
    if (!s || !p) return -1;
    memset(s,0,sizeof(*s));
    for (i=0;i<5;i++) memcpy(s->points[i],p,sizeof(s->points[i]));
    for (i=0;i<4;i++) {
        SmN64TrailSegment *q=&s->segment[i];
        q->color=0x2e000000u;q->fade=8;q->width=400;q->first=(uint8_t)(i==0);
        for (j=0;j<3;j++) {
            uint32_t c=(color>>(j*8))&255u;
            c-=((3u-i)*(c/4u));
            q->color|=c<<(j*8);
            q->initial_vertex_rgb[j]=(uint8_t)(c<128u?c*2u:255u);
        }
    }
    return 1;
}
int smn64_trail_append(SmN64Trail *s, const int32_t p[3]) {
    unsigned i,n;
    if (!s || !p || s->head>=5 || s->delete_requested) return -1;
    s->head++;if(s->head==5)s->head=0;
    memcpy(s->points[s->head],p,sizeof(s->points[0]));n=s->head;
    for(i=4;i>0;i--) {
        SmN64TrailSegment *q=&s->segment[i-1];
        memcpy(q->to,s->points[n],sizeof(q->to));
        n=n==0?4:n-1;
        memcpy(q->from,s->points[n],sizeof(q->from));
    }
    return 1;
}
int smn64_trail_tick(SmN64Trail *s) {
    unsigned i,j;int all=1;
    if(!s)return -1;
    if(s->delete_requested)return 1;
    if(s->stopping) {
        for(i=0;i<4;i++) {
            SmN64TrailSegment *q=&s->segment[i];
            uint32_t c=q->color,out=c&0xff000000u;
            if(!(c&0xffffffu))continue;
            all=0;
            for(j=0;j<3;j++) {
                uint32_t v=(c>>(j*8))&255u;
                /* Original uses signed halfword threshold, low-byte subtract. */
                int32_t step=q->fade<32768u?(int32_t)q->fade:(int32_t)q->fade-65536;
                v=(int32_t)v<step?0u:(v-(q->fade&255u))&255u;
                out|=v<<(j*8);
            }
            q->color=out;
        }
        if(all)s->delete_requested=1;
    }
    /* Source segment frame_count(+51) is zero for these line objects. The
     * fractional clock still advances by 128 in the subsequent list pass. */
    for(i=0;i<4;i++) {
        s->segment[i].fraction=(uint8_t)(s->segment[i].fraction+128u);
        s->segment[i].frame=0;
    }
    return s->delete_requested?1:0;
}
int smn64_trails_start(SmN64TrailPair *p,uint32_t color,const SmN64TrailHost *h) {
    unsigned i;int32_t point[3];SmN64Trail *s;
    if(!p)return -1;
    for(i=0;i<2;i++)if(!p->trail[i]) {
        if(!h||!h->marker||!h->allocate||h->marker(h->context,5u+i,point)!=1)return -2;
        s=h->allocate(h->context);if(!s)return -2;
        if(s==p->trail[1u-i])return -2;
        smn64_trail_init(s,point,color);p->trail[i]=s;
    }
    return 1;
}
void smn64_trails_stop(SmN64TrailPair *p) {
    unsigned i;if(!p)return;
    for(i=0;i<2;i++)if(p->trail[i]) {p->trail[i]->stopping=1;p->trail[i]=0;}
}
int smn64_trails_retain(SmN64TrailPair *p,const SmN64TrailHost *h) {
    unsigned i;int32_t point[3];if(!p)return -1;
    for(i=0;i<2;i++)if(p->trail[i]) {
        if(!h||!h->marker||h->marker(h->context,5u+i,point)!=1)return -2;
        if(smn64_trail_append(p->trail[i],point)!=1)return -1;
    }
    return 1;
}
