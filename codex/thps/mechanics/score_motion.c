#include "score_motion.h"
#include <limits.h>
#include <math.h>
static int32_t word(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int32_t asr12(int32_t x){return x>=0?x/4096:-1-(int32_t)(~(uint32_t)x>>12);}
int32_t thps1_score_grind_distance(const int32_t d[3]){
 uint32_t sum=0;unsigned i;
 for(i=0;i<3;i++){
  uint32_t low=(uint32_t)asr12(d[i])&65535u;
  int32_t n=low<32768u?(int32_t)low:(int32_t)low-65536;
  sum+=(uint32_t)(n*n);
 }
 /* Invalid sqrt/cvt.w.s yields MIPS integer-indefinite. Never cast NaN in C. */
 if(sum>INT32_MAX)return INT32_MIN;
 return (int32_t)sqrtf((float)(int32_t)sum);
}
void thps1_score_boost_step(int32_t *ticks,const int32_t v[3],int32_t a[3]){
 volatile float x=(float)v[0]/4096.f*((float)v[0]/4096.f);
 volatile float y=(float)v[1]/4096.f*((float)v[1]/4096.f);
 volatile float z=(float)v[2]/4096.f*((float)v[2]/4096.f);
 volatile float xy=x+y,xyz=xy+z,scaled=xyz*4096.f;
 int32_t speed,divisor;unsigned i;
 if(!*ticks)return;
 speed=scaled>0?(int32_t)sqrtf((float)(int32_t)scaled)*64:0;
 divisor=(speed<4096?4096:speed)>>12;
 for(i=0;i<3;i++)a[i]=word((uint32_t)a[i]+(uint32_t)(v[i]/divisor));
 *ticks=word((uint32_t)*ticks-1u);
}
