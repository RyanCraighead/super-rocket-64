#include "ground_kernels.h"
/* MIPS arithmetic shifts floor negatives; signed division truncates to zero.
 * Reinterpret wrapped low words without implementation-defined unsigned casts. */
static int32_t signed_word(uint32_t n) { return n <= 0x7fffffffU ? (int32_t)n : -1-(int32_t)(~n); }
static int32_t add(int32_t a,int32_t b) { return signed_word((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return signed_word((uint32_t)a-(uint32_t)b); }
static int32_t mul(int32_t a,int32_t b) { return signed_word((uint32_t)a*(uint32_t)b); }
static int32_t asr(int32_t a,unsigned n) { return a>=0 ? (int32_t)((uint32_t)a>>n) : -1-(int32_t)((~(uint32_t)a)>>n); }
/* 0x800581b0..0x800581e8; normal totals have no wrap. */
int32_t thps1_ground_speed_from_total(int32_t total) { return mul(total,184320)/28; }
/* 0x8004ff78..0x80050020, state==0 only. */
int32_t thps1_ground_steer_step(int32_t dt,int32_t mode,int down) {
 int32_t coeff=mode==1?10240:mode==3?15872:30720;
 int32_t step=asr(mul(dt,coeff),8); return down?add(step,step):step;
}
/* 0x8005021c..0x80050448. limit=184320, or368640 with down input. */
int32_t thps1_ground_steer_rate(int32_t old,int32_t step,int32_t limit,int32_t x,int left,int right,int down) {
 if(x && !left && !right) {
  int32_t target=mul(x,limit)/128;
  if(target < -limit) target=-limit;
  if(target > limit) target=limit;
  if(old < target) {
   int32_t delta=add(step,mul(step,asr(sub(target,old),12))/(int32_t)((uint32_t)limit>>13));
   old=add(old,delta); if(old>target) old=target;
  }
  if(old > target) {
   int32_t delta=add(step,mul(step,asr(sub(old,target),12))/(int32_t)((uint32_t)limit>>13));
   old=sub(old,delta); if(old<target) old=target;
  }
 } else if(left) { old=sub(old,step); if(old < -limit) old=-limit; }
 else if(right) { old=add(old,step); if(old>limit) old=limit; }
 else { old=sub(old,asr(old,down?1:2)); if(asr(add(old,2048),12)==0)old=0; }
 return old;
}
/* 0x8005a550..0x8005a5ac. Ground braking snaps to state7/zero when
 * speed<=threshold AND threshold==40960. Larger threshold can suppress braking. */
int32_t thps1_ground_brake_threshold(int32_t normal_y) {
 int32_t n=-normal_y; if(n<768)n=768;
 int32_t threshold=mul(sub(4096,n),6144)/208;
 return threshold<40960?40960:threshold;
}
/* 0x8005a614..0x8005a794. Brake gate: ground,state0,not bailed and
 * ((input+B0 && !left && !right) || (analogY>0 && analogX==0)). */
int32_t thps1_ground_brake_component(int32_t velocity,int32_t y,int32_t dt) {
 int32_t loss=y>0?mul(velocity,y/16)/128:asr(velocity,4);
 return sub(velocity,asr(mul(loss,dt),8));
}
/* 0x80052bf0..0x80052c88 and 0x8005b830..0x8005b870 respectively. */
int32_t thps1_ground_integrate_position(int32_t p,int32_t v,int32_t a,int32_t dt,int32_t dt2) {
 return add(p,add(asr(mul(v,dt),8),asr(mul(a,dt2),8)/2));
}
int32_t thps1_ground_integrate_velocity(int32_t v,int32_t a,int32_t dt) {return add(v,asr(mul(a,dt),8));}
/* 0x8005b938..0x8005bbf4, only called when speed>cap. */
int32_t thps1_ground_cap_component(int32_t v,int32_t cap,int32_t speed) {return mul(v,asr(cap,12))/asr(speed,12);}
