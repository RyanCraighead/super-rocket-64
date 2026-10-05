/* THPS1 USA Rev1 landing predicates. Exact ranges and exclusions in
 * landing_evidence.md. No collision, animation, or gameplay guesses here. */
#include "landing_checks.h"
#include <math.h>
#include <string.h>
static int32_t word(uint32_t x) { return x<=0x7fffffffU?(int32_t)x:-1-(int32_t)~x; }
static int32_t sub(int32_t a,int32_t b) { return word((uint32_t)a-(uint32_t)b); }
static int32_t neg(int32_t a) { return word(0U-(uint32_t)a); }
int32_t thps1_landing_mul(int32_t a,int32_t b) {
    volatile float x=(float)b*0.000000059604644775390625f;
    volatile float y=(float)a*x;
    volatile float z=y*4096.0f;
    return (int32_t)z;
}
int32_t thps1_landing_dot(const int32_t a[3],const int32_t b[3]) {
    volatile float ax=(float)a[0]*0.000244140625f, ay=(float)a[1]*0.000244140625f, az=(float)a[2]*0.000244140625f;
    volatile float bx=(float)b[0]*0.000244140625f, by=(float)b[1]*0.000244140625f, bz=(float)b[2]*0.000244140625f;
    volatile float x=ax*bx, y=ay*by, z=az*bz;
    volatile float xy=x+y, xyz=xy+z, scaled=xyz*4096.0f;
    return (int32_t)scaled;
}
int32_t thps1_landing_speed(const int32_t v[3]) {
    int32_t d=thps1_landing_dot(v,v);
    volatile float f=(float)(uint32_t)d;
    return word((uint32_t)(int32_t)sqrtf(f)<<6);
}
void thps1_landing_project(const int32_t v[3],const int32_t n[3],int32_t out[3]) {
    int32_t d=thps1_landing_dot(v,n); unsigned i;
    for(i=0;i<3;i++)out[i]=sub(v[i],thps1_landing_mul(d,n[i]));
}
int thps1_landing_trick_bails(int active,int interruptible,uint32_t flags,int flip,int grab,int rotation,int32_t angle,int32_t direction) {
    int bad;
    if(!active && !rotation)return 0;
    bad=((flags&0x2000U)!=0 && flip)||((flags&0x4000U)!=0 && grab)||!interruptible;
    if(rotation) {
        uint32_t wrapped=(uint32_t)angle&4095U;
        if(direction>0) { if(wrapped<3796U)bad=1; }
        else if(wrapped>=301U)bad=1;
    }
    return bad!=0;
}
void thps1_landing_check(const Thps1LandingInput *in,Thps1LandingResult *out) {
    unsigned i;int32_t d;
    memset(out,0,sizeof(*out));
    thps1_landing_project(in->velocity,in->normal,out->tangent_velocity);
    out->tangent_speed=thps1_landing_speed(out->tangent_velocity);
    if(in->already_bailed) { out->reason=THPS1_LANDING_ALREADY_BAILED;return; }
    if(out->tangent_speed>122880) {
        d=thps1_landing_dot(out->tangent_velocity,in->forward);
        out->alignment_q12=word((uint32_t)d<<6)/(out->tangent_speed/64);
        d=out->alignment_q12;
        if(d<0)d=neg(d);
        if(d<1638) {
            out->reason=THPS1_LANDING_ALIGNMENT;
            out->side_dot=thps1_landing_dot(out->tangent_velocity,in->side);
            if(in->stance_flags&2)out->side_dot=neg(out->side_dot);
            if(out->side_dot<=0) { out->bail_phase_override=5;out->bail_clip_override=46; }
            return;
        }
    }
    if(in->source_state!=2 && thps1_landing_dot(in->up,in->normal)<2000 && in->up[1]>=-1999) {
        out->reason=THPS1_LANDING_UPSIDE_DOWN;
        out->bail_phase_override=9;out->bail_clip_override=56;
        /* 80054334..543d8 swaps forward=-old up; then velocity=-forward*10. */
        for(i=0;i<3;i++)out->override_velocity[i]=word((uint32_t)in->up[i]*10U);
        out->override_velocity[1]=0;
        return;
    }
    if(thps1_landing_trick_bails(in->trick_active,in->trick_interruptible,in->trick_flags,in->flip_held,in->grab_held,in->rotation_active,in->rotation_angle,in->rotation_direction))out->reason=THPS1_LANDING_UNFINISHED_TRICK;
}
void thps1_landing_source_origin(const int32_t hit[3],const int32_t n[3],int32_t out[3]) {
    unsigned i;for(i=0;i<3;i++)out[i]=word((uint32_t)hit[i]+(uint32_t)n[i]*30U);
}

static int32_t asr(int32_t a,unsigned n) { return a>=0?(int32_t)((uint32_t)a>>n):-1-(int32_t)(~(uint32_t)a>>n); }
static void cross(const int32_t a[3],const int32_t b[3],int32_t out[3]) {
    unsigned i;for(i=0;i<3;i++) {
        unsigned j=(i+1)%3,k=(i+2)%3;
        out[i]=asr(word((uint32_t)a[j]*(uint32_t)b[k]-(uint32_t)a[k]*(uint32_t)b[j]),12);
    }
}
static void normalize(const int32_t in[3],int32_t out[3]) {
    volatile float x=(float)in[0]*0.000244140625f,y=(float)in[1]*0.000244140625f,z=(float)in[2]*0.000244140625f;
    volatile float xx=x*x,yy=y*y,zz=z*z,xy=xx+yy,sum=xy+zz;
    volatile float length=sqrtf(sum),inv,nx,ny,nz,scaled;
    if(length==0) {out[0]=out[1]=out[2]=0;return;}
    inv=1.0f/length;nx=x*inv;ny=y*inv;nz=z*inv;
    scaled=nx*4096.0f;out[0]=(int32_t)scaled;
    scaled=ny*4096.0f;out[1]=(int32_t)scaled;
    scaled=nz*4096.0f;out[2]=(int32_t)scaled;
}
void thps1_landing_alignment_basis(const int32_t previous[3],const int32_t earlier[3],const int32_t normal[3],int32_t forward[3],int32_t side[3],int32_t up[3]) {
    int32_t d[3],n[3],total=0;unsigned i;
    for(i=0;i<3;i++) {d[i]=sub(previous[i],earlier[i]);total=word((uint32_t)total+(uint32_t)(d[i]<0?neg(d[i]):d[i]));}
    if(total>32768)for(i=0;i<3;i++)d[i]=asr(d[i],4);
    normalize(d,n);
    for(i=0;i<3;i++) {forward[i]=neg(n[i]);up[i]=normal[i];}
    cross(forward,up,side);cross(up,side,forward);
}
void thps1_landing_upside_basis(const int32_t old_forward[3],const int32_t old_up[3],int32_t new_forward[3],int32_t new_up[3]) {
    int32_t f[3],u[3];unsigned i;
    for(i=0;i<3;i++) {f[i]=neg(old_up[i]);u[i]=old_forward[i];}
    for(i=0;i<3;i++) {new_forward[i]=f[i];new_up[i]=u[i];}
}

static int32_t neg_low16(int32_t v) {
    uint32_t n=(0U-(uint32_t)v)&65535U;
    return n<=32767U?(int32_t)n:(int32_t)n-65536;
}
int thps1_ground_stance_reorient(uint16_t clip,int32_t longitudinal[3],int32_t side[3],const int32_t velocity[3],uint32_t *flags) {
    unsigned i;
    if(clip==34 || thps1_landing_dot(longitudinal,velocity)<=20480)return 0;
    for(i=0;i<3;i++) {longitudinal[i]=neg_low16(longitudinal[i]);side[i]=neg_low16(side[i]);}
    *flags^=2U;
    return 1;
}

int thps1_landing_cleanup_ready(int32_t airborne_latch,int32_t previous_state,int32_t current_state) {
    return airborne_latch!=0 && previous_state!=0 && current_state==0;
}
void thps1_landing_start_animation(ThpsAnim *a,const ThpsAnimBank *bank,int crouched) {
    a->rate=65536;
    thps1_anim_run(a,bank,crouched?25:5,crouched?3:0,-1,-1);
}
int thps1_landing_finish_animation(ThpsAnim *a,const ThpsAnimBank *bank,int crouched) {
    if(!a->finished || (a->id!=5 && a->id!=25))return 0;
    a->rate=65536;
    if(crouched)thps1_anim_run(a,bank,8,19,26,19);
    else thps1_anim_run(a,bank,0,0,-1,-1);
    return 1;
}
void thps1_landing_history_shift(const int32_t current[3],int32_t previous[3],int32_t earlier[3]) {
    int32_t c[3],p[3];unsigned i;
    for(i=0;i<3;i++) {c[i]=current[i];p[i]=previous[i];}
    for(i=0;i<3;i++) {earlier[i]=p[i];previous[i]=c[i];}
}

static int32_t low16(int32_t v) {
    uint32_t n=(uint32_t)v&65535U;
    return n<=32767U?(int32_t)n:(int32_t)n-65536;
}
void thps1_landing_clean_basis(const int32_t old_longitudinal[3],const int32_t normal[3],int32_t new_longitudinal[3],int32_t new_side[3],int32_t new_up[3]) {
    int32_t f[3],n[3],s[3],raw[3],input_normal[3];unsigned i;
    for(i=0;i<3;i++) {f[i]=low16(old_longitudinal[i]);input_normal[i]=low16(normal[i]);}
    normalize(input_normal,n);cross(f,n,raw);normalize(raw,s);cross(n,s,f);
    for(i=0;i<3;i++) {new_longitudinal[i]=low16(f[i]);new_side[i]=low16(s[i]);new_up[i]=low16(n[i]);}
}
