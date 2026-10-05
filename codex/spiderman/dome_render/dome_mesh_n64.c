/* Portable source submission, no ROM/vertex/texture payload embedded. */
#include "dome_mesh_n64.h"
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#ifdef __FAST_MATH__
#error "Source dome arithmetic forbids fast-math"
#endif
#if FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128 || DBL_MANT_DIG != 53
#error "Source dome arithmetic requires IEEE binary32"
#endif
_Static_assert(sizeof(SmN64DomeRenderVertex)==16,"Source Vtx layout");
_Static_assert(sizeof(SmN64DomeCorner)==8,"Source corner layout");
static uint16_t u16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]*256u+p[1]);}
static int16_t s16(uint16_t n){return n<=INT16_MAX?(int16_t)n:(int16_t)((int32_t)n-65536);}
static int shape(uint16_t slot,uint16_t node,uint16_t *v,uint16_t *c,uint16_t *tex){
    if(slot==165&&node<4){*v=node==1?93:node==3?16:30;*c=node==1?315:node==3?45:90;*tex=node==1?402:403;return 1;}
    if((slot==166||slot==226)&&node==0){*v=74;*c=324;*tex=404;return 1;}
    if(slot==248&&node==0){*v=127;*c=450;*tex=403;return 1;}
    if(slot==249&&node<5){*v=node==2?16:30;*c=node==2?45:90;*tex=403;return 1;}
    return 0;
}
int smn64_dome_geometry_decode(uint16_t slot,uint16_t node,const uint8_t *vb,size_t vn,
    const uint8_t *cb,size_t cn,SmN64DomeGeometry *out){
    uint16_t v,c,t;
    if(!vb||!cb||!out||!shape(slot,node,&v,&c,&t)||vn!=(size_t)v*16u||cn!=(size_t)c*8u)return 0;
    SmN64DomeGeometry g;memset(&g,0,sizeof g);g.model_slot=slot;g.node=node;g.texture_slot=t;g.vertex_count=v;g.corner_count=c;
    for(size_t i=0;i<v;++i){const uint8_t *p=vb+i*16;SmN64DomeRenderVertex *x=&g.vertices[i];
        x->x=s16(u16(p));x->y=s16(u16(p+2));x->z=s16(u16(p+4));x->flag=s16(u16(p+6));
        x->s=s16(u16(p+8));x->t=s16(u16(p+10));memcpy(x->rgba,p+12,4);}
    for(size_t i=0;i<c;++i){const uint8_t *p=cb+i*8;SmN64DomeCorner *x=&g.corners[i];
        x->pool=u16(p);x->s=s16(u16(p+2));x->t=s16(u16(p+4));x->matrix=u16(p+6);
        if(x->pool>=v||x->matrix)return 0;}
    *out=g;return 1;
}
int smn64_dome_mesh_submit(const SmN64DomeGeometry *g,const SmN64DomeRenderVertex *pool,
    size_t n,const int32_t matrix[16],SmN64DomeDraw *out){
    uint16_t v,c,t;
    if(!g||!pool||!matrix||!out||!shape(g->model_slot,g->node,&v,&c,&t)||n!=v||
      g->vertex_count!=v||g->corner_count!=c||g->texture_slot!=t||matrix[3]||matrix[7]||matrix[11]||matrix[15]!=65536)return 0;
    SmN64DomeDraw d;memset(&d,0,sizeof d);d.model_slot=g->model_slot;d.node=g->node;d.texture_slot=t;d.corner_count=c;
    memcpy(d.model_s16_16,matrix,sizeof d.model_s16_16);
    for(size_t i=0;i<c;++i){const SmN64DomeCorner *x=&g->corners[i];if(x->pool>=n||x->matrix)return 0;
        d.vertices[i]=pool[x->pool];d.vertices[i].s=x->s;d.vertices[i].t=x->t;}
    *out=d;return 1;
}
static float fm(float a,float b){volatile float f=a*b;return f;}
static float ff(float a){volatile float f=a;return f;}
static int fixed(float f,int32_t *out){float q=fm(f,65536.0f);
    if(!isfinite(q)||q < -2147483648.0f||q>2147483648.0f)return 0;
    *out=q==2147483648.0f?INT32_MAX:(int32_t)q;return 1;}
int smn64_dome_zero_rotation_matrix(const int32_t p[3],uint16_t flags,const int16_t s[3],int32_t out[16]){
    if(!p||!s||!out||fegetround()!=FE_TONEAREST)return 0;
    int32_t m[16]={0};m[15]=65536;
    for(size_t i=0;i<3;++i){float scale=(flags&0x200u)?fm(ff((float)s[i]),0x1p-12f):1.0f;
        if(!fixed(fm(scale,0x1p-4f),&m[i*5]))return 0;
        if(!fixed(fm(fm(ff((float)p[i]),0x1p-12f),0x1p-4f),&m[12+i]))return 0;}
    memcpy(out,m,sizeof m);return 1;
}

int smn64_dome_host_projection(const float p[16],float out[16]){
    static const unsigned zero[]={1,2,3,4,6,7,12,13,15};
    if(!p||!out||fegetround()!=FE_TONEAREST)return 0;
    for(unsigned i=0;i<16;++i)if(!isfinite(p[i]))return 0;
    for(unsigned i=0;i<sizeof zero/sizeof zero[0];++i)if(p[zero[i]]!=0)return 0;
    if(p[0]<=0||p[5]<=0||p[10]>=-1||p[11]!=-1||p[14]>=0)return 0;
    double near_plane=(double)p[14]/((double)p[10]-1.0);
    double far_plane=(double)p[14]/((double)p[10]+1.0);
    double biased_near=near_plane*(double)1.05f;
    if(!(near_plane>0&&far_plane>biased_near))return 0;
    float result[16];memcpy(result,p,sizeof result);
    result[10]=(float)(-(far_plane+biased_near)/(far_plane-biased_near));
    result[14]=(float)(-(2.0*far_plane*biased_near)/(far_plane-biased_near));
    if(!isfinite(result[10])||!isfinite(result[14]))return 0;
    memcpy(out,result,sizeof result);return 1;
}
