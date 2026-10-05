/* Independently expressed original USA1.0 sprite projection arithmetic.
 * No original machine-code, texture payload or precomputed frame table. */
#include "sprite_projection_n64.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <fenv.h>
#include <string.h>
#ifdef __FAST_MATH__
#error "Original sprite arithmetic forbids fast-math"
#endif
#if FLT_RADIX!=2 || FLT_MANT_DIG!=24 || FLT_MAX_EXP!=128 || DBL_MANT_DIG!=53
#error "Original sprite arithmetic requires IEEE binary32 and binary64"
#endif
static float f32(float x){volatile float v=x;return v;}
static float fm(float a,float b){volatile float v=a*b;return v;}
static float fa(float a,float b){volatile float v=a+b;return v;}
static float fd(float a,float b){volatile float v=a/b;return v;}
static double dm(double a,double b){volatile double v=a*b;return v;}
static double da(double a,double b){volatile double v=a+b;return v;}
static double ds(double a,double b){volatile double v=a-b;return v;}
/* Source libultra sine polynomial/reduction constants, expressed as exact
 * mathematical binary64 literals, not copied instructions or a lookup table. */
static double sine_poly(double r){
    double q=dm(r,r);
    double p=da(dm(0x1.5dbdf0e314bfep-19,q),-0x1.9f6ffeea56814p-13);
    p=da(dm(p,q),0x1.110ed3804c2a0p-7);
    p=da(-0x1.55554bc83656dp-3,dm(p,q));
    return da(dm(dm(r,q),p),r);
}
static float source_sine(float x){
    uint32_t bits;memcpy(&bits,&x,4);unsigned size=(bits>>22)&0x1ffu;
    if(size<0xffu){if(size<0xe6u)return x;return f32((float)sine_poly((double)x));}
    double a=(double)x,q=dm(a,0x1.45f306dc9c883p-2);
    int n=(int)(q>=0?da(q,.5):ds(q,.5));
    double r=ds(ds(a,dm((double)n,0x1.921fb50000000p+1)),dm((double)n,0x1.110b4611a6263p-25));
    float v=f32((float)sine_poly(r));return n&1?-v:v;
}
static float source_cosine(float x){
    double a=(double)(x>0?x:-x);
    double q=da(dm(a,0x1.45f306dc9c883p-2),.5);
    int n=(int)(q>=0?da(q,.5):ds(q,.5));
    double k=ds((double)n,.5);
    double r=ds(ds(a,dm(k,0x1.921fb50000000p+1)),dm(k,0x1.110b4611a6263p-25));
    float v=f32((float)sine_poly(r));return n&1?-v:v;
}
static float source_angle(int16_t angle){return f32((float)dm((double)f32((float)angle),-0x1.91eb851eb851fp-10));}
static int fixed_cell(float value,int32_t *out){
    float scaled=fm(value,65536.0f);
    if(!isfinite(scaled)||scaled< -2147483648.0f||scaled>2147483648.0f)return 0;
    /* Exact positive endpoint follows observed original-MIPS trunc.w.s.
     * This out-of-game-range endpoint is not N64 FPU hardware certification. */
    *out=scaled==2147483648.0f?INT32_MAX:(int32_t)scaled;return 1;
}
int smn64_sprite_project(const SmN64SpriteInput *in,const float camera[16],SmN64SpriteQuad *out){
    if(!in||!camera||!out||fegetround()!=FE_TONEAREST||!isfinite(in->divisor80)||in->divisor80<=0
        ||!in->texture_width||!in->texture_height||in->texture_width>1023||in->texture_height>1023)return 0;
    for(int k=0;k<16;++k)if(!isfinite(camera[k])||fabsf(camera[k])>32768.0f)return 0;
    if(camera[3]!=0||camera[7]!=0||camera[11]!=0||camera[15]!=1)return 0;
    float matrix[16];memcpy(matrix,camera,sizeof matrix);
    const float angle=source_angle(in->angle84);
    if(angle!=0){const float sn=source_sine(angle),cs=source_cosine(angle);
        for(int row=0;row<3;++row){float x=matrix[row],y=matrix[4+row];
            matrix[row]=fa(fm(cs,x),fm(sn,y));matrix[4+row]=fa(fm(-sn,x),fm(cs,y));}}
    const float ratio=fd(f32((float)in->size56),in->divisor80);
    float scale[3]={fd(fm(fm(ratio,f32((float)in->texture_width)),.0625f),10.0f),
                    fd(fm(fm(ratio,f32((float)in->texture_height)),.0625f),10.0f),1};
    for(int col=0;col<3;++col)for(int row=0;row<3;++row)matrix[col*4+row]=fm(matrix[col*4+row],scale[col]);
    for(int row=0;row<3;++row)matrix[12+row]=fm(fm(f32((float)in->position_fixed12[row]),0x1p-12f),0x1p-4f);
    SmN64SpriteQuad q;memset(&q,0,sizeof q);
    for(int k=0;k<16;++k)if(!fixed_cell(matrix[k],&q.model_s16_16[k]))return 0;
    const int16_t corners[4][3]={{-10,-10,0},{10,-10,0},{10,10,0},{-10,10,0}};
    const uint8_t indices[6]={0,1,2,0,2,3};
    memcpy(q.xyz,corners,sizeof corners);memcpy(q.indices,indices,sizeof indices);
    q.st[0][1]=q.st[1][1]=(int16_t)(in->texture_height<<5);
    q.st[1][0]=q.st[2][0]=(int16_t)(in->texture_width<<5);
    for(int i=0;i<4;++i){for(int c=0;c<3;++c)q.rgba[i][c]=in->logical_rgb[c]>=128?255:(uint8_t)(in->logical_rgb[c]*2u);q.rgba[i][3]=in->alpha;}
    q.texture_slot=in->texture_slot;*out=q;return 1;
}
int smn64_web_knot_quad(const int32_t p[3],const float camera[16],const uint8_t rgb[3],uint8_t alpha,SmN64SpriteQuad *out){
    if(!p||!rgb)return 0;
    SmN64SpriteInput in;memset(&in,0,sizeof in);memcpy(in.position_fixed12,p,sizeof in.position_fixed12);memcpy(in.logical_rgb,rgb,3);
    in.size56=90;in.divisor80=400;in.texture_width=in.texture_height=24;in.texture_slot=46;in.alpha=alpha;
    return smn64_sprite_project(&in,camera,out);
}
int smn64_sprite_camera_from_host_view(const float view[16],float out[16]){
    if(!view||!out)return 0;
    for(int i=0;i<16;++i)if(!isfinite(view[i]))return 0;
    if(view[3]!=0||view[7]!=0||view[11]!=0||view[15]!=1)return 0;
    for(int a=0;a<3;++a)for(int b=a;b<3;++b){double dot=0;for(int row=0;row<3;++row)dot+=(double)view[a*4+row]*view[b*4+row];if(fabs(dot-(a==b?1.0:0.0))>.001)return 0;}
    float result[16]={0};result[15]=1;
    for(int col=0;col<3;++col)for(int row=0;row<3;++row)result[col*4+row]=view[row*4+col]*(row==0?1.0f:-1.0f);
    memcpy(out,result,sizeof result);return 1;
}
#ifdef SMN64_SPRITE_TESTING
void smn64_sprite_test_trig(int16_t angle,float out[2]){float a=source_angle(angle);out[0]=source_sine(a);out[1]=source_cosine(a);}
#endif
