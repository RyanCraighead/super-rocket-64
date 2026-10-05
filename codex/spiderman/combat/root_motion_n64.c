#include "root_motion_n64.h"
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return s32((uint32_t)a*(uint32_t)b);}
int smn64_combo_root_resolve(SmN64CombatRoot *s,const SmN64Combo *combo,const SmN64CombatMotion *m,const SmN64RootHost *h){
    int32_t origin[3],to[3],from[3],move[3];unsigned i,anchor;int rc;
    if(!s||!combo||!m||!h)return -1;
    if(!m->anchored&&!m->forward_units)return 0;
    if(!h->refresh_pose||!h->actor_sweep||!h->world_trace||!h->marker)return -2;
    rc=h->refresh_pose(h->context,combo);if(rc!=1)return -2;
    memcpy(origin,s->position,sizeof(origin));origin[1]=add(origin[1],131072);
    for(i=0;i<3;i++)to[i]=sub(origin[i],mul(s->forward[i],32));
    rc=h->actor_sweep(h->context,origin,to,4096);if(rc<0)return -2;if(rc)return 0;
    for(i=0;i<3;i++)to[i]=sub(sub(origin[i],mul(s->right[i],32)),mul(s->forward[i],128));
    rc=h->world_trace(h->context,origin,to);if(rc<0)return -2;if(rc)return 0;
    for(i=0;i<3;i++)to[i]=sub(add(origin[i],mul(s->right[i],32)),mul(s->forward[i],128));
    rc=h->world_trace(h->context,origin,to);if(rc<0)return -2;if(rc)return 0;
    for(i=0;i<3;i++){from[i]=add(origin[i],mul(s->up[i],64));to[i]=sub(from[i],mul(s->forward[i],128));}
    rc=h->world_trace(h->context,from,to);if(rc<0)return -2;if(rc)return 0;
    if(m->anchored){
        switch(m->anchor_bone){case 6:anchor=0;break;case 5:anchor=1;break;case 1:anchor=2;break;case 0:anchor=3;break;default:return -1;}
        rc=h->marker(h->context,(uint8_t)m->anchor_bone,move);if(rc!=1)return -2;
        for(i=0;i<3;i++)move[i]=i==1&&!m->include_y?0:sub(move[i],s->retained_markers[anchor][i]);
    }else for(i=0;i<3;i++)move[i]=mul(s->forward[i],m->forward_units);
    for(i=0;i<3;i++)s->position[i]=sub(s->position[i],move[i]);
    return 1;
}
int smn64_combo_root_retain(SmN64CombatRoot *s,const SmN64RootHost *h){
    static const uint8_t ids[4]={6,5,1,0};int32_t points[4][3];unsigned i;
    if(!s||!h||!h->marker)return -1;
    for(i=0;i<4;i++)if(h->marker(h->context,ids[i],points[i])!=1)return -2;
    memcpy(s->retained_markers,points,sizeof(points));return 1;
}
