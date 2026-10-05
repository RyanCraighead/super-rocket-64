#include "dome_shatter_n64.h"
#include "../web/resource_n64.h"
#include "../graphics/allocation_order_n64.h"
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#if defined(__FAST_MATH__)
#error Original shatter requires strict binary32 operations
#endif
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
#error Original shatter requires IEEE binary32
#endif
static float fmul(float a,float b){volatile float x=a*b;return x;}
static float fadd(float a,float b){volatile float x=a+b;return x;}
static float fdiv(float a,float b){volatile float x=a/b;return x;}
static int32_t s32(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int16_t s16(uint32_t x){x&=65535u;return x<=32767u?(int16_t)x:(int16_t)(-1-(int32_t)(65535u-x));}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int32_t sar(int32_t a,unsigned b){uint32_t x=(uint32_t)a;return s32((x>>b)|(a<0?~(UINT32_MAX>>b):0));}
static int to_fixed(float x,int32_t *out){float y=fmul(x,4096.f);if(!isfinite(y)||y<-2147483648.f||y>=2147483648.f)return 0;*out=(int32_t)y;return 1;}
int smn64_dome_shatter_fragment_init(SmN64DomeShatterFragment *out,const float xyz[3][3],const int32_t v[3],int32_t floor,uint32_t rng[3]){
    SmN64DomeShatterFragment s;uint32_t r[3];unsigned i,j;
    if(!out||!xyz||!v||!rng||fegetround()!=FE_TONEAREST)return -1;
    memset(&s,0,sizeof(s));memcpy(r,rng,sizeof(r));
    for(i=0;i<3;i++)for(j=0;j<3;j++)if(!to_fixed(xyz[i][j],&s.base_corners[i][j]))return -1;
    memcpy(s.base_corners[3],s.base_corners[2],sizeof(s.base_corners[3]));
    memcpy(s.velocity,v,sizeof(s.velocity));s.floor_y=floor;s.gray=128;s.alive=1;
    s.lifetime=(uint16_t)(1u+smn64_web_random(r,30));
    *out=s;memcpy(rng,r,sizeof(r));return 1;
}
int smn64_dome_shatter_spawn(const SmN64DomeGeometry *g,const float translation[3],int32_t floor,uint32_t rng[3],uint64_t *clock,SmN64DomeShatterFragment out[SMN64_DOME_SHATTER_COUNT]){
    SmN64DomeShatterFragment result[SMN64_DOME_SHATTER_COUNT];uint32_t r[3];uint64_t next,base;unsigned i,j,k;
    if(!g||!translation||!rng||!clock||!out||fegetround()!=FE_TONEAREST||g->model_slot!=249||g->node!=4||g->texture_slot!=403||g->vertex_count!=30||g->corner_count!=90)return -1;
    for(j=0;j<3;j++)if(!isfinite(translation[j]))return -1;
    for(i=0;i<90;i++)if(g->corners[i].pool>=g->vertex_count||g->corners[i].matrix)return -1;
    memcpy(r,rng,sizeof(r));next=*clock;if(smn64_graphical_reserve(&next,SMN64_DOME_SHATTER_COUNT,&base)!=1)return -1;
    for(i=0;i<SMN64_DOME_SHATTER_COUNT;i++){
        float xyz[3][3],local[3],length,speed;int32_t velocity[3];const SmN64DomeRenderVertex *first=&g->vertices[g->corners[i*3].pool];
        local[0]=(float)first->x;local[1]=(float)first->y;local[2]=(float)first->z;
        length=sqrtf(fadd(fadd(fmul(local[0],local[0]),fmul(local[1],local[1])),fmul(local[2],local[2])));
        if(length!=0.f){float inv=fdiv(1.f,length);for(j=0;j<3;j++)local[j]=fmul(local[j],inv);}
        speed=(float)(30u+smn64_web_random(r,30));
        for(j=0;j<3;j++)if(!to_fixed(fmul(local[j],speed),&velocity[j]))return -1;
        for(k=0;k<3;k++){
            const SmN64DomeRenderVertex *v=&g->vertices[g->corners[i*3+k].pool];
            const int16_t pos[3]={v->x,v->y,v->z};
            for(j=0;j<3;j++)xyz[k][j]=fadd((float)pos[j],fmul(translation[j],16.f));
        }
        if(smn64_dome_shatter_fragment_init(&result[i],(const float (*)[3])xyz,velocity,floor,r)!=1)return -1;
        result[i].graphical_serial=base+i;
    }
    memcpy(out,result,sizeof(result));memcpy(rng,r,sizeof(r));*clock=next;return 1;
}
int smn64_dome_shatter_fragment_tick(SmN64DomeShatterFragment *s,uint32_t rng[3]){
    unsigned i,j;int32_t correction;if(!s||!rng||!s->lifetime||s->lifetime>30)return -1;if(!s->alive)return 0;
    for(i=0;i<4;i++)for(j=0;j<3;j++)s->base_corners[i][j]=add(s->base_corners[i][j],s->velocity[j]);
    s->velocity[1]=add(s->velocity[1],8192);
    if(s->base_corners[0][1]>s->floor_y){
        correction=s32((uint32_t)s->floor_y-(uint32_t)s->base_corners[0][1]);s->base_corners[0][1]=s->floor_y;
        for(i=1;i<4;i++)s->base_corners[i][1]=add(s->base_corners[i][1],correction);
        s->velocity[1]=sar(s32(0u-(uint32_t)s->velocity[1]),2);
        s->velocity[0]=sar(s->velocity[0],1);s->velocity[2]=sar(s->velocity[2],1);
    }
    memcpy(s->corners,s->base_corners,sizeof(s->corners));
    for(i=0;i<4;i++)for(j=0;j<3;j+=2){uint32_t jitter=(smn64_web_random(rng,41)-20u)<<12;s->corners[i][j]=add(s->corners[i][j],s32(jitter));}
    s->age=s16((uint32_t)(int32_t)s->age+1u);
    if(s->age>=(int32_t)s->lifetime){s->alive=0;return 0;}
    s->gray=(uint8_t)(((int32_t)s->lifetime-s->age)*128/(int32_t)s->lifetime);return 1;
}
int smn64_dome_shatter_snapshot(const SmN64DomeShatterFragment *s,SmN64DomeShatterDraw *out){
    static const unsigned order[4]={1,0,2,3};static const int16_t st[4][2]={{0,0},{2048,0},{2048,2048},{0,2048}};static const uint8_t indices[6]={0,1,2,0,2,3};SmN64DomeShatterDraw q;unsigned i,j;uint8_t alpha;
    if(!s||!out)return -1;
    if(!s->alive)return 0;
    memset(&q,0,sizeof(q));alpha=(uint8_t)(((s->gray-1u)&127u)<<1);
    for(i=0;i<4;i++){for(j=0;j<3;j++){q.submitted.xyz[i][j]=s16((uint32_t)sar(s->corners[order[i]][j],13));q.submitted.rgba[i][j]=255;}q.submitted.rgba[i][3]=alpha;memcpy(q.submitted.st[i],st[i],sizeof(st[i]));}
    memcpy(q.submitted.indices,indices,sizeof(indices));for(i=0;i<3;i++)q.submitted.model_s16_16[i*5]=8192;q.submitted.model_s16_16[15]=65536;q.submitted.texture_slot=403;
    q.environment_rgba[3]=alpha;q.combiner[0]=0xfc50d3ff;q.combiner[1]=0xfffffe38;q.render_mode=0x0c184b50;q.graphical_serial=s->graphical_serial;*out=q;return 1;
}
