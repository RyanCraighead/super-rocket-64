#include "swing_contact_n64.h"
#include <string.h>

static int32_t i32(uint32_t v) {
    return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);
}
static int query(SmN64SwingContact *s,SmN64SwingContactRay kind,
                 SmN64SwingTrace trace,void *context) {
    SmN64SwingContactQuery q;
    memset(&q,0,sizeof(q));
    memcpy(q.start,s->line_start,sizeof(q.start));
    memcpy(q.end,s->line_end,sizeof(q.end));
    q.kind=kind;q.arg1=1;q.arg4=1;
    /* Source line reset clears presence, surface and distance, but retains
     * previous position and normal until a real hit overwrites them. */
    s->line.hit=0;s->line.surface=0;s->line.distance=INT32_MAX;
    s->line.surface_flags=0;
    return trace(context,&q,&s->line)==1;
}
int smn64_swing_contact_run(SmN64SwingContact *out,const SmN64Swinger *swinger,
                            SmN64SwingMarker marker,SmN64SwingTrace trace,
                            void *context) {
    if(!out || !swinger || !marker || !trace)return -1;
    SmN64SwingContact s=*out;
    int32_t old[3],before[3],after[3];
    memcpy(old,s.position,sizeof(old));
    if(marker(context,&s,2,before)!=1)return -2;
    smn64_swinger_endpoint(swinger,s.position);
    if(marker(context,&s,2,after)!=1)return -2;
    /* Only horizontal movement is doubled. Original Y remains the newly
     * evaluated marker Y, even when the body traverses a large vertical arc. */
    after[0]=i32((uint32_t)after[0]+(uint32_t)after[0]-(uint32_t)before[0]);
    after[2]=i32((uint32_t)after[2]+(uint32_t)after[2]-(uint32_t)before[2]);
    memcpy(s.line_start,before,sizeof(before));
    memcpy(s.line_end,after,sizeof(after));
    if(!query(&s,SMN64_SWING_CONTACT_FORWARD,trace,context))return -3;
    uint16_t contact=0;
    if(s.line.hit)contact=s.line.normal[1]<-2600?2:1;
    else {
        memcpy(s.line_start,after,sizeof(after));
        memcpy(s.line_end,after,sizeof(after));
        s.line_end[1]=i32((uint32_t)s.line_end[1]+UINT32_C(0x80000));
        if(!query(&s,SMN64_SWING_CONTACT_DOWN,trace,context))return -3;
        if(s.line.hit)contact=2;
        else {
            s.line_end[1]=i32((uint32_t)s.line_start[1]-UINT32_C(0x20000));
            if(!query(&s,SMN64_SWING_CONTACT_UP,trace,context))return -3;
            if(s.line.hit)contact=1;
        }
    }
    if(contact) {
        s.collision|=contact;
        for(int i=0;i<3;i++) {
            s.position[i]=i32((uint32_t)s.line.position[i]+
                (uint32_t)(int32_t)s.line.normal[i]*(uint32_t)s.body_offset);
            s.velocity[i]=0;
        }
    } else {
        for(int i=0;i<3;i++)
            s.velocity[i]=i32((uint32_t)s.position[i]-(uint32_t)old[i]);
    }
    *out=s;return 1;
}
