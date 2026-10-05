#include "swinger_n64.h"
#include <math.h>
#include <string.h>

static float f32(float x) { volatile float r=x; return r; }
static int32_t i32(uint32_t x) {
    return x<=INT32_MAX ? (int32_t)x : -1-(int32_t)(UINT32_MAX-x);
}
static int16_t i16(uint32_t x) {
    x &= UINT32_C(65535);
    return x<=INT16_MAX ? (int16_t)x : (int16_t)(-1-(int32_t)(65535-x));
}
static int32_t sar(int32_t x,unsigned n) {
    uint32_t u=(uint32_t)x;
    if(x<0)u=(u>>n)|(UINT32_MAX<<(32-n));else u>>=n;
    return i32(u);
}
static int32_t mullo(int32_t a,int32_t b) {
    return i32((uint32_t)a*(uint32_t)b);
}
/* Original raw-application finite-angle polynomial, 800018D0/80001CB0.
 * A host sinf/cosf can differ by one ULP and cross a fixed12 truncation edge.
 * Our input is always a finite signed-angle product, bounded by 51 radians. */
static double f64(double x) {volatile double r=x;return r;}
static float sine_polynomial(double x) {
    double z=f64(x*x);
    double p=f64(f64(0x1.5dbdf0e314bfep-19*z)-0x1.9f6ffeea56814p-13);
    p=f64(f64(p*z)+0x1.110ed3804c2a0p-7);
    p=f64(-0x1.55554bc83656dp-3+f64(p*z));
    return f32((float)f64(f64(f64(x*z)*p)+x));
}
static float original_sine(float angle) {
    double x=(double)angle;
    if(fabsf(angle)<0x1.8p+0f) {
        if(fabsf(angle)<0x1p-12f)return angle;
        return sine_polynomial(x);
    }
    double q=f64(x*0x1.45f306dc9c883p-2);
    int n=(int)(q>=0.0?f64(q+0.5):f64(q-0.5));
    x=f64(x-f64((double)n*0x1.921fb5p+1));
    x=f64(x-f64((double)n*0x1.110b4611a6263p-25));
    float r=sine_polynomial(x);return (n&1)?-r:r;
}
static float original_cosine(float angle) {
    double x=(double)fabsf(angle);
    double q=f64(f64(x*0x1.45f306dc9c883p-2)+0.5);
    int n=(int)f64(q+0.5);q=f64((double)n-0.5);
    x=f64(x-f64(q*0x1.921fb5p+1));
    x=f64(x-f64(q*0x1.110b4611a6263p-25));
    float r=sine_polynomial(x);return (n&1)?-r:r;
}
static int32_t cosine(int32_t phase) {
    if(!phase)return 4096;
    float rad=f32((float)(phase%4096)*0x1.921fb4p-10f);
    return (int32_t)f32(original_cosine(rad)*4096.0f);
}
/* 800AA428 -> 800BFF88 -> 800BFA98. This is NOT libc acosf: the
 * source uses sqrt(1.0001-x*x), then its unnormalized atan approximation. */
static int16_t initial_pitch(int32_t dot) {
    uint32_t mag=dot<0 ? 0U-(uint32_t)dot : (uint32_t)dot;
    float x=f32((float)i32(mag)*0x1p-12f);
    float square=f32(0x1.00068ep+0f-f32(x*x));
    float y=square<0.0f ? 1.0f : sqrtf(square);
    float hi=fabsf(x),lo=fabsf(y);int swap=hi<lo;
    if(swap){float t=hi;hi=lo;lo=t;}
    float r=f32(lo/hi);
    float a=f32(f32(r*0x1.921fb6p-1f)+f32(f32(fabsf(f32(lo-hi))*0x1.3c6a7ep-2f)*r));
    if(x>=0.0f){if(swap)a=f32(0x1.921fb6p+0f-a);}
    else if(swap)a=f32(a+0x1.921fb6p+0f);
    else a=f32(0x1.921fb6p+1f-a);
    return i16((uint32_t)(int32_t)f32(a*0x1.45f306p+9f));
}
static void normalize(int32_t v[3]) {
    float x=f32((float)v[0]*0x1p-12f),y=f32((float)v[1]*0x1p-12f);
    float z=f32((float)v[2]*0x1p-12f);
    float n=sqrtf(f32(f32(f32(x*x)+f32(y*y))+f32(z*z)));
    if(n==0.0f){v[0]=v[1]=v[2]=0;return;}
    float inv=f32(1.0f/n);
    v[0]=(int32_t)f32(f32(x*inv)*4096.0f);
    v[1]=(int32_t)f32(f32(y*inv)*4096.0f);
    v[2]=(int32_t)f32(f32(z*inv)*4096.0f);
}
static void cross(const int32_t a[3],const int32_t b[3],int32_t out[3]) {
    for(int i=0;i<3;i++) {
        int j=(i+1)%3,k=(i+2)%3;
        out[i]=sar(i32((uint32_t)mullo(a[j],b[k])-(uint32_t)mullo(a[k],b[j])),12);
    }
}
void smn64_swinger_basis(const int32_t position[3],const int32_t anchor[3],
                        const int32_t normal[3],int16_t out[9],int32_t *vertical_abs) {
    int32_t up[3],right[3],forward[3];
    for(int i=0;i<3;i++)up[i]=sar(i32((uint32_t)position[i]-(uint32_t)anchor[i]),12);
    normalize(up);cross(up,normal,right);normalize(right);cross(up,right,forward);
    for(int i=0;i<3;i++) {
        out[3*i]=i16((uint32_t)right[i]);out[3*i+1]=i16((uint32_t)up[i]);
        out[3*i+2]=i16((uint32_t)forward[i]);
    }
    if(vertical_abs)*vertical_abs=up[1]<0?-up[1]:up[1];
}
static void columns(SmN64Swinger *s,const int16_t m[9]) {
    for(int i=0;i<3;i++) {
        s->forward[i]=m[3*i+2];s->right[i]=m[3*i];
        s->negative_up[i]=-(int32_t)m[3*i+1];
    }
}
void smn64_swinger_init(SmN64Swinger *s,const int32_t anchor[3],int32_t length,
                       const int16_t basis[9],const int32_t normal[3],uint32_t now) {
    SmN64Swinger n;memset(&n,0,sizeof(n));
    n.last_tick=now;n.length=length;
    memcpy(n.anchor,anchor,sizeof(n.anchor));
    memcpy(n.initial_basis,basis,sizeof(n.initial_basis));
    memcpy(n.basis,basis,sizeof(n.basis));columns(&n,basis);
    uint32_t dot=0;
    for(int i=0;i<3;i++)dot+=(uint32_t)sar(mullo(n.negative_up[i],normal[i]),12);
    n.pitch=initial_pitch(i32(dot));n.phase=2048;
    /* 80053D00 interprets length as unsigned before the float32 sqrt. */
    int32_t root=(int32_t)sqrtf(f32((float)(uint32_t)length));
    n.phase_rate=(120-root)/2;n.field_188=length/8;n.field_18c=9;
    *s=n;
}
static void arc_rotation(int16_t pitch,int16_t roll,int16_t m[9]) {
    float x=f32((float)pitch*0x1.921fb6p-10f);
    float z=f32((float)roll*0x1.921fb6p-10f);
    float sx=original_sine(x),cx=original_cosine(x),sz=original_sine(z),cz=original_cosine(z);
    /* 800521A0 with Y angle zero. Retain each source float32 boundary. */
    float a[9]={cz,-sz,0.0f,f32(cx*sz),f32(cx*cz),-sx,
                f32(sx*sz),f32(sx*cz),cx};
    for(int i=0;i<9;i++)m[i]=i16((uint32_t)(int32_t)f32(a[i]*4096.0f));
}
void smn64_swinger_step(SmN64Swinger *s,uint32_t now) {
    s->phase=i32((uint32_t)s->phase+(now-s->last_tick)*(uint32_t)s->phase_rate);
    s->last_tick=now;
    int16_t pitch=i16((uint32_t)sar(mullo(s->pitch,cosine(s->phase)+4096),12));
    int16_t r[9];arc_rotation(pitch,s->roll,r);
    /* 80053648 converts each quantized matrix to float, multiplies in the
     * original order, then truncates back to fixed12. Do not replace this with
     * an integer matrix product; operation boundaries are observably different. */
    for(int i=0;i<3;i++)for(int j=0;j<3;j++) {
        float t[3];
        for(int k=0;k<3;k++)
            t[k]=f32(f32((float)s->initial_basis[3*i+k]*0x1p-12f)*
                     f32((float)r[3*k+j]*0x1p-12f));
        float sum=f32(f32(t[0]+t[1])+t[2]);
        s->basis[3*i+j]=i16((uint32_t)(int32_t)f32(sum*4096.0f));
    }
    columns(s,s->basis);
}
void smn64_swinger_endpoint(const SmN64Swinger *s,int32_t out[3]) {
    for(int i=0;i<3;i++)out[i]=i32((uint32_t)s->anchor[i]-(uint32_t)mullo(s->length,s->negative_up[i]));
}
int smn64_swinger_complete(const SmN64Swinger *s) { return s->phase>=4096; }
int16_t smn64_swinger_animation_frame(const SmN64Swinger *s,int32_t last_frame) {
    return i16((uint32_t)sar(mullo(i32((uint32_t)s->phase-2048U),last_frame),11));
}
void smn64_swinger_set_roll(SmN64Swinger *s,int32_t roll) {s->roll=i16((uint32_t)roll);}
int32_t smn64_swinger_phase_bias(const SmN64Swinger *s) {
    uint32_t a=(uint32_t)s->negative_up[1];if(s->negative_up[1]<0)a=0U-a;
    uint32_t quadrant=((uint32_t)s->phase&4095U)>>10;
    int32_t value=i32((quadrant==1 || quadrant==2)?4096U-a:a-4096U);
    return value/6;
}
