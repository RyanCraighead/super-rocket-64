#include "strand_n64.h"
#include "resource_n64.h"
#include "../movement/locomotion_n64.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int32_t si(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int32_t add(int32_t a,int32_t b){return si((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return si((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return si((uint32_t)a*(uint32_t)b);}
static int32_t sar(int32_t x,unsigned n){uint32_t v=(uint32_t)x;return si((v>>n)|((v&0x80000000u)?UINT32_MAX<<(32-n):0u));}
static int32_t shift(int32_t x,unsigned n){return si((uint32_t)x<<n);}
static int16_t narrow(int32_t v){uint32_t x=(uint32_t)v&65535u;return x<=32767?(int16_t)x:(int16_t)(-1-(int32_t)(65535-x));}
static void perpendicular(const int32_t from[3],const int32_t to[3],const int32_t camera[3],int32_t out[3]){
    int32_t a[3],b[3];
    for(int i=0;i<3;i++){a[i]=sar(sub(to[i],from[i]),12);b[i]=sub(camera[i],sar(from[i],12));}
    if(a[0]>=501||a[1]>=501||a[2]>=501)for(int i=0;i<3;i++)a[i]=sar(a[i],4);
    uint32_t square=0;
    for(int i=0;i<3;i++){
        int j=(i+1)%3,k=(i+2)%3;out[i]=sub(mul(a[j],b[k]),mul(a[k],b[j]));
        int32_t v=narrow(sar(out[i],8));square+=(uint32_t)v*(uint32_t)v;
    }
    int32_t length=(int32_t)sqrtf((float)square);
    if(length<5){memset(out,0,3*sizeof(*out));return;}
    for(int i=0;i<3;i++)out[i]=shift(out[i]/length,4);
}
int32_t smn64_strand_point_count(const int32_t from[3],const int32_t to[3]){
    uint32_t square=0;for(int i=0;i<3;i++){int32_t v=narrow(sar(sub(to[i],from[i]),12));square+=(uint32_t)v*(uint32_t)v;}
    int32_t n=(int32_t)sqrtf((float)square)/80;return n<1?1:(n>40?40:n);
}
int smn64_strand_endpoints(SmN64Strand *s,const int32_t a[3],const int32_t b[3],const int32_t camera[3]){
    if(!s||!a||!b||!camera||s->count<1||s->count>40)return -1;
    int32_t delta[3],p[3];
    for(int i=0;i<3;i++){p[i]=a[i];s->anchor[i]=a[i];delta[i]=sub(b[i],a[i])/s->count;}
    for(int j=0;j<s->count;j++)for(int i=0;i<3;i++){p[i]=add(p[i],delta[i]);s->primary[j][i]=p[i];s->point[j].base[i]=p[i];}
    perpendicular(a,b,camera,s->perpendicular);return 1;
}
int smn64_strand_init(SmN64Strand *s,const int32_t a[3],const int32_t b[3],const int32_t camera[3],uint32_t rng[3]){
    if(!s||!a||!b||!camera||!rng)return -1;
    memset(s,0,sizeof(*s));s->count=smn64_strand_point_count(a,b);s->bend_decay=1;
    for(int i=0;i<s->count;i++){
        SmN64StrandPoint *p=&s->point[i];p->amplitude=4;p->rate=(uint8_t)(smn64_web_random(rng,100)+125);
        p->phase=(uint16_t)smn64_web_random(rng,4096);p->interpolation=(uint8_t)(smn64_web_random(rng,192)+64);p->offset=(int8_t)((int32_t)smn64_web_random(rng,19)-9);
    }
    return smn64_strand_endpoints(s,a,b,camera);
}
int smn64_strand_step(SmN64Strand *s,uint32_t now,uint32_t rng[3]){
    if(!s||!rng||s->count<1||s->count>40)return -1;
    if(s->wobble)for(int i=0;i<s->count;i++){
        SmN64StrandPoint *p=&s->point[i];int32_t phase=si((uint32_t)p->phase+(uint32_t)p->rate*now);
        int32_t amount=sar(mul(p->amplitude,smn64_locomotion_sin(phase)),12);
        for(int j=0;j<3;j++)s->primary[i][j]=add(p->base[j],mul(s->perpendicular[j],amount));
    }
    if(s->bend_trigger)s->bend=20;
    if(s->bend){
        int32_t pulse=sar(mul(s->bend,smn64_locomotion_sin(si(now<<9))),12);
        for(int i=0;i<s->count;i++){
            int32_t a=sar(mul(pulse,smn64_locomotion_sin(((i+1)*2048)/s->count)),12);
            for(int j=0;j<3;j++)s->primary[i][j]=add(s->point[i].base[j],mul(s->perpendicular[j],a));
        }
        s->bend_trigger=0;s->bend=s->bend<s->bend_decay?0:(uint8_t)(s->bend-s->bend_decay);
    }
    int32_t previous[3];memcpy(previous,s->anchor,sizeof(previous));
    for(int i=0;i<s->count;i++){
        SmN64StrandPoint *p=&s->point[i];if(s->jitter){p->interpolation=(uint8_t)smn64_web_random(rng,256);p->offset=(int8_t)((int32_t)smn64_web_random(rng,19)-9);}
        for(int j=0;j<3;j++){
            int32_t middle=add(previous[j],sar(mul(sub(s->primary[i][j],previous[j]),p->interpolation),8));
            s->particles[i][j]=middle;
            int32_t base=i?previous[j]:sar(add(s->anchor[j],middle),1);
            s->secondary[2*i][j]=add(base,mul(p->offset,s->perpendicular[j]));s->secondary[2*i+1][j]=middle;previous[j]=s->primary[i][j];
        }
    }
    return 1;
}
