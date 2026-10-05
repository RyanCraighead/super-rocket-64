#include "lifecycle_n64.h"
#include "../movement/locomotion_n64.h"
#include "../graphics/allocation_order_n64.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int32_t si(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int32_t add(int32_t a,int32_t b){return si((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return si((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return si((uint32_t)a*(uint32_t)b);}
static int32_t sar(int32_t x,unsigned n){uint32_t v=(uint32_t)x;return si((v>>n)|((v&0x80000000u)?UINT32_MAX<<(32-n):0u));}
static int32_t shl(int32_t x,unsigned n){return si((uint32_t)x<<n);}
static int16_t s16(int32_t v){uint32_t x=(uint32_t)v&65535u;return x<=32767?(int16_t)x:(int16_t)(-1-(int32_t)(65535-x));}
static int32_t distance(const int32_t a[3],const int32_t b[3],int narrow){
    uint32_t sum=0;for(int i=0;i<3;i++){int32_t v=sar(sub(a[i],b[i]),12);if(narrow)v=s16(v);sum+=(uint32_t)v*(uint32_t)v;}return (int32_t)sqrtf((float)sum);
}
static int valid_strand(const SmN64WebVisualStrand *s){return s->alive&&s->has_geometry&&s->geometry.count>=1&&s->geometry.count<=40;}
static int free_strand(const SmN64WebVisuals *v){for(int i=0;i<SMN64_WEB_VISUAL_CAPACITY;i++)if(!v->strands[i].alive)return i;return -1;}
static int free_splat(const SmN64WebVisuals *v){for(int i=0;i<SMN64_WEB_VISUAL_CAPACITY;i++)if(!v->splats[i].alive)return i;return -1;}
static void newest(int32_t order[SMN64_WEB_VISUAL_CAPACITY],uint32_t *count,int i){
    uint32_t n=0;while(n<*count&&order[n]!=i)n++;
    if(n==*count)(*count)++;
    while(n>0){order[n]=order[n-1];n--;}order[0]=i;
}
static void fresh(SmN64WebVisualStrand *s,int kind){memset(s,0,sizeof(*s));s->alive=1;s->kind=(uint8_t)kind;s->particle_alpha=255;s->first_update=1;for(int i=0;i<3;i++){s->line_rgb[i]=162;s->particle_rgb[i]=128;}}
void smn64_web_visuals_init(SmN64WebVisuals *v,SmN64WebVisualMarker marker,void *ctx){memset(v,0,sizeof(*v));v->active_zip=v->active_swing=-1;v->marker=marker;v->marker_context=ctx;}
static void corners(SmN64WebSplat *s){
    for(int i=0;i<3;i++){
        int32_t a=mul(s->radius,s->axis_u[i]),b=mul(s->radius,s->axis_v[i]);
        s->corners[0][i]=sub(sub(s->center[i],a),b);s->corners[1][i]=sub(add(s->center[i],a),b);
        s->corners[2][i]=add(sub(s->center[i],a),b);s->corners[3][i]=add(add(s->center[i],a),b);
    }
}
void smn64_web_splat_init(SmN64WebSplat *s,const int32_t p[3],const int32_t input[3],uint16_t rotation){
    memset(s,0,sizeof(*s));int32_t n[3],a[3],b[3];uint32_t mag[3];
    for(int i=0;i<3;i++){n[i]=s16(input[i]);mag[i]=n[i]<0?0u-(uint32_t)n[i]:(uint32_t)n[i];s->center[i]=add(p[i],mul(input[i],10));s->rgb[i]=128;}
    if(mag[0]<=mag[1]&&mag[0]<=mag[2]){a[0]=0;a[1]=-n[2];a[2]=n[1];}
    else if(mag[1]<=mag[0]&&mag[1]<=mag[2]){a[0]=n[2];a[1]=0;a[2]=-n[0];}
    else{a[0]=-n[1];a[1]=n[0];a[2]=0;}
    for(int i=0;i<3;i++){int j=(i+1)%3,k=(i+2)%3;b[i]=sar(sub(mul(n[j],a[k]),mul(n[k],a[j])),12);}
    int32_t sn=smn64_locomotion_sin(rotation),cs=smn64_locomotion_cos(rotation);
    for(int i=0;i<3;i++){
        s->axis_u[i]=sar(add(mul(a[i],cs),mul(b[i],sn)),12);
        s->axis_v[i]=sar(add(mul(a[i],-sn),mul(b[i],cs)),12);
    }
    s->rotation=rotation;s->target_radius=32;s->alive=1;s->drip=input[1]==0;
    (void)smn64_web_splat_step(s,0.0f);
}
int smn64_web_splat_step(SmN64WebSplat *s,float seconds){
    if(!s||!isfinite(seconds)||seconds<0.0f||seconds>1.0f)return -1;
    if(!s->alive)return 1;
    int32_t difference=sub(s->target_radius,s->radius);
    if(difference>=2){s->radius=add(s->radius,sar(difference,1));corners(s);}
    else if(s->drip){
        volatile float movement=seconds*122880.0f;int32_t d=(int32_t)movement;
        for(int i=0;i<4;i++)if(sub(s->corners[i][1],s->center[1])>0)s->corners[i][1]=add(s->corners[i][1],d);
    }
    s->age=(uint16_t)(s->age+1u);
    if(s16(s->age)>30)for(int i=0;i<3;i++)s->rgb[i]=s->rgb[i]<5?0:(uint8_t)(s->rgb[i]-5);
    if(!(s->rgb[0]|s->rgb[1]|s->rgb[2]))s->alive=0;
    return 1;
}
int smn64_web_visual_lifecycle(void *context,SmN64WebEventKind op,SmN64WebPlayer *p,const SmN64Swinger *sw){
    SmN64WebVisuals *v=context;if(!v||!p||v->strand_count>SMN64_WEB_VISUAL_CAPACITY||v->splat_count>SMN64_WEB_VISUAL_CAPACITY)return -1;
    if(op==SMN64_WEB_CREATE_ZIP){
        if(p->resource.web_type)return -2;
        int i=free_strand(v);if(i<0)return -3;
        newest(v->strand_order,&v->strand_count,i);fresh(&v->strands[i],SMN64_VISUAL_ZIP);v->strand_graphical_base[i]=0;v->active_zip=i;return 1;
    }
    if(op==SMN64_WEB_FIRE_ZIP){
        if(v->active_zip<0||v->active_zip>=SMN64_WEB_VISUAL_CAPACITY||!v->marker)return -1;
        SmN64WebVisualStrand *s=&v->strands[v->active_zip];int j=free_splat(v);int32_t from[3];
        if(!s->alive||s->kind!=SMN64_VISUAL_ZIP)return -1;
        if(j<0)return -3;
        if(v->marker(v->marker_context,p,0,from)!=1)return -4;
        uint64_t serial;uint32_t allocations=1;
        if(!s->has_geometry)allocations+=(uint32_t)smn64_strand_point_count(p->target,from)+2u;
        if(smn64_graphical_reserve(&v->graphical_clock,allocations,&serial)!=1)return -3;
        v->splat_graphical_serial[j]=serial;
        if(!s->has_geometry)v->strand_graphical_base[v->active_zip]=serial+1u;
        uint16_t angle=(uint16_t)smn64_web_random(p->random_state,4096);newest(v->splat_order,&v->splat_count,j);smn64_web_splat_init(&v->splats[j],p->target,p->target_normal,angle);
        memcpy(s->from,from,sizeof(from));memcpy(s->to,p->target,sizeof(s->to));
        int rc=s->has_geometry?smn64_strand_endpoints(&s->geometry,s->to,s->from,v->camera):smn64_strand_init(&s->geometry,s->to,s->from,v->camera,p->random_state);
        s->has_geometry=1;s->phase=1;return rc;
    }
    if(op==SMN64_WEB_CREATE_SWINGER){
        if(!sw)return -1;
        if(p->resource.web_type)return -2;
        int i=free_strand(v);if(i<0)return -3;
        int32_t end[3];uint64_t serial;smn64_swinger_endpoint(sw,end);
        if(smn64_graphical_reserve(&v->graphical_clock,(uint32_t)smn64_strand_point_count(sw->anchor,end)+2u,&serial)!=1)return -3;
        newest(v->strand_order,&v->strand_count,i);fresh(&v->strands[i],SMN64_VISUAL_SWING);SmN64WebVisualStrand *s=&v->strands[i];v->strand_graphical_base[i]=serial;
        int rc=smn64_strand_init(&s->geometry,sw->anchor,end,v->camera,p->random_state);s->geometry.wobble=1;s->has_geometry=1;v->active_swing=i;return rc;
    }
    if(op==SMN64_WEB_DETACH_STRAND){
        if(v->active_swing<0||v->active_swing>=SMN64_WEB_VISUAL_CAPACITY)return -1;
        int i=free_strand(v);if(i<0)return -3;
        SmN64WebVisualStrand *old=&v->strands[v->active_swing],*s=&v->strands[i];int32_t end[3];
        if(!valid_strand(old))return -1;
        for(int a=0;a<3;a++)end[a]=add(old->geometry.primary[old->geometry.count-1][a],old->attachment_offset[a]);
        uint64_t serial;if(smn64_graphical_reserve(&v->graphical_clock,(uint32_t)smn64_strand_point_count(old->geometry.anchor,end)+2u,&serial)!=1)return -3;
        v->strand_graphical_base[i]=serial;
        newest(v->strand_order,&v->strand_count,i);fresh(s,SMN64_VISUAL_RELEASED);int rc=smn64_strand_init(&s->geometry,old->geometry.anchor,end,v->camera,p->random_state);s->geometry.wobble=1;s->has_geometry=1;return rc;
    }
    if(op==SMN64_WEB_DELETE_SWINGER){if(v->active_swing>=0&&v->active_swing<SMN64_WEB_VISUAL_CAPACITY)v->strands[v->active_swing].alive=0;v->active_swing=-1;return 1;}
    if(op==SMN64_WEB_RETRACT_ZIP){if(v->active_zip<0||v->active_zip>=SMN64_WEB_VISUAL_CAPACITY)return -1;
        v->strands[v->active_zip].phase=3;v->active_zip=-1;return 1;}
    return -1;
}
int smn64_web_visual_lifecycle_ordered(void *context,SmN64WebEventKind op,SmN64WebPlayer *p,const SmN64Swinger *sw,uint64_t *clock){
    SmN64WebVisuals *v=context;
    if(!v||smn64_graphical_import(&v->graphical_clock,clock)!=1)return -1;
    int rc=smn64_web_visual_lifecycle(context,op,p,sw);
    if(rc>0)*clock=v->graphical_clock;
    return rc;
}
int smn64_web_visuals_swing_endpoint(SmN64WebVisuals *v,const SmN64Swinger *sw){
    if(!v||!sw)return -1;
        if(v->active_swing<0)return 1;
        if(v->active_swing>=SMN64_WEB_VISUAL_CAPACITY)return -1;
    if(!valid_strand(&v->strands[v->active_swing]))return -1;
    int32_t p[3];smn64_swinger_endpoint(sw,p);return smn64_strand_endpoints(&v->strands[v->active_swing].geometry,sw->anchor,p,v->camera);
}
int smn64_web_visuals_attach(SmN64WebVisuals *v,const SmN64WebPlayer *p){
    if(!v||!p||v->strand_count>SMN64_WEB_VISUAL_CAPACITY||v->splat_count>SMN64_WEB_VISUAL_CAPACITY)return -1;
        if(v->active_swing<0&&v->active_zip<0)return 1;
        if(!v->marker)return -4;
    int32_t a[3];
    if(v->active_swing>=0){
        if(v->active_swing>=SMN64_WEB_VISUAL_CAPACITY)return -1;
        SmN64WebVisualStrand *s=&v->strands[v->active_swing];
        if(!valid_strand(s))return -1;
        if(v->marker(v->marker_context,p,p->anim.animation==280?1:0,a)!=1)return -4;
        for(int i=0;i<3;i++){a[i]=sub(a[i],mul(p->forward[i],8));s->attachment_offset[i]=sub(a[i],s->geometry.primary[s->geometry.count-1][i]);}
        if(smn64_strand_endpoints(&s->geometry,s->geometry.anchor,a,v->camera)<0)return -1;
    }
    if(v->active_zip>=0){
        if(v->active_zip>=SMN64_WEB_VISUAL_CAPACITY)return -1;
        SmN64WebVisualStrand *s=&v->strands[v->active_zip];
        if(!valid_strand(s))return -1;
        if(v->marker(v->marker_context,p,0,a)!=1)return -4;
        memcpy(s->from,a,sizeof(a));
        if(smn64_strand_endpoints(&s->geometry,s->to,s->from,v->camera)<0)return -1;
    }
    return 1;
}
int smn64_web_visuals_effects(SmN64WebVisuals *v,const int32_t player[3],uint32_t now,float seconds,uint32_t rng[3]){
    if(!v||!player||!rng||v->strand_count>SMN64_WEB_VISUAL_CAPACITY||v->splat_count>SMN64_WEB_VISUAL_CAPACITY||!isfinite(seconds)||seconds<0.0f||seconds>1.0f)return -1;
    for(uint32_t order=0;order<v->splat_count;order++){
        int i=v->splat_order[order];if(i<0||i>=SMN64_WEB_VISUAL_CAPACITY)return -1;
        if(v->splats[i].alive&&smn64_web_splat_step(&v->splats[i],seconds)<0)return -1;
    }
    for(uint32_t order=0;order<v->strand_count;order++){
        int i=v->strand_order[order];if(i<0||i>=SMN64_WEB_VISUAL_CAPACITY)return -1;
        SmN64WebVisualStrand *s=&v->strands[i];if(!s->alive||!s->has_geometry)continue;
        if(!valid_strand(s))return -1;
        if(s->kind==SMN64_VISUAL_RELEASED){
            int32_t end[3],goal[3],delta[3];memcpy(end,s->geometry.point[s->geometry.count-1].base,sizeof(end));memcpy(goal,s->geometry.anchor,sizeof(goal));
            int32_t d=distance(goal,end,0);if(s->released_close)d=sub(d,700);goal[1]=add(goal[1],shl(d,12));
            for(int a=0;a<3;a++){delta[a]=sub(goal[a],end[a]);end[a]=add(end[a],sar(delta[a],4));}
            int32_t zero[3]={0,0,0};if(distance(delta,zero,1)<500)s->released_close=1;
            if(smn64_strand_endpoints(&s->geometry,s->geometry.anchor,end,v->camera)<0)return -1;
            if(smn64_strand_step(&s->geometry,now,rng)<0)return -1;
            for(int a=0;a<3;a++){if(s->line_rgb[a]>10)s->line_rgb[a]-=10;if(s->particle_rgb[a]>10)s->particle_rgb[a]-=10;}
            s->particle_alpha=(uint8_t)(s->particle_rgb[0]>=128?255:s->particle_rgb[0]*2);
            s->age=(uint16_t)(s->age+1u);if(distance(end,player,1)>2000||s16(s->age)>60)s->alive=0;
        }else{
            if(smn64_strand_step(&s->geometry,now,rng)<0)return -1;
        }
    }
    return 1;
}

int smn64_web_visuals_zip_actors(SmN64WebVisuals *v,uint32_t now){
    if(!v||v->strand_count>SMN64_WEB_VISUAL_CAPACITY)return -1;
    for(uint32_t order=0;order<v->strand_count;order++){
        int i=v->strand_order[order];if(i<0||i>=SMN64_WEB_VISUAL_CAPACITY)return -1;
        SmN64WebVisualStrand *s=&v->strands[i];
        if(!s->alive||s->kind!=SMN64_VISUAL_ZIP)continue;
        int32_t elapsed=s->first_update?2:si(now-s->last_tick);
        if(elapsed>=7)elapsed=6;
        s->first_update=0;s->last_tick=now;
        if(s->phase!=3)continue;
        if(!valid_strand(s))return -1;
        s->timer=(uint16_t)(s->timer+(uint16_t)elapsed);
        if(s->timer>16){s->phase=4;s->alive=0;continue;}
        int32_t a[3],c=smn64_locomotion_cos((int32_t)s->timer*64);
        for(int j=0;j<3;j++)a[j]=sub(s->to[j],mul(sar(sub(s->to[j],s->from[j]),12),c));
        if(smn64_strand_endpoints(&s->geometry,a,s->to,v->camera)<0)return -1;
    }
    return 1;
}
