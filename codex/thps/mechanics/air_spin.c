#include "air_spin.h"
#include <string.h>
/* Define MIPS low-word arithmetic without signed-overflow or shift UB. */
static int32_t bits(uint32_t v) { return v <= INT32_MAX ? (int32_t)v : -1-(int32_t)(UINT32_MAX-v); }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return bits((uint32_t)a-(uint32_t)b); }
static int32_t mul(int32_t a,int32_t b) { return bits((uint32_t)a*(uint32_t)b); }
static int32_t asr(int32_t v,unsigned n) { uint32_t u=(uint32_t)v; return bits((u>>n)|(v<0 ? UINT32_MAX<<(32-n) : 0)); }
static int32_t halfword(int32_t v) { uint32_t u=(uint32_t)v&65535u; return u<32768 ? (int32_t)u : (int32_t)u-65536; }
static int32_t approach_frame(int32_t frame,int32_t target) {
 int32_t d=sub(target,frame), sign=d<0 ? -1 : 1;
 if (!d) return frame;
 if (d<0) d=sub(0,d);
 return halfword(add(frame,sign*(d<4 ? 1 : d<13 ? 3 : 5)));
}
static void play(Thps1AirSpin *s,Thps1AirSpinOutput *o,int32_t id,int32_t from,int32_t to,int32_t continuation) {
 s->animation_rate=65536;
 o->animation_changed=1; o->animation=id; o->from=from; o->to=to; o->continuation=continuation;
 /* Run's detailed state is owned by air_animation; these fields are its
  * visible valid-ID outputs with the original bank, not a fabricated loop. */
 s->animation=id; s->frame=from;
}
int thps1_air_spin(Thps1AirSpin *s,Thps1AirSpinInput *in,Thps1AirSpinOutput *o) {
 int32_t step, target, left, right, rate, frame, id, scale;
 const int32_t limit=655360;
 if (!s->state) return 0;
 memset(o,0,sizeof(*o));
 if (s->state==1 || s->state==2) {
  if (!s->input_lock) {
   if (in->edge51) { in->edge51=0; if(s->queued180>0)s->queued180=0; s->queued180=sub(s->queued180,1); }
   if (in->edge71) { in->edge71=0; if(s->queued180<0)s->queued180=0; s->queued180=add(s->queued180,1); }
  }
  step=asr(mul(in->dt8,150),8);
  if (s->queued180>0) {
   o->queued_rotation=add(o->queued_rotation,step); ++o->rotation_calls;
   s->queued_angle=add(s->queued_angle,step);
   if (s->queued_angle>=2048) {
    s->queued180=sub(s->queued180,1); s->queued_angle=sub(s->queued_angle,2048);
    if (!s->count_lock) s->completed180=add(s->completed180,1);
   }
  }
  if (s->queued180<0) {
   o->queued_rotation=sub(o->queued_rotation,step); ++o->rotation_calls;
   s->queued_angle=sub(s->queued_angle,step);
   if (s->queued_angle<=-2048) {
    s->queued180=add(s->queued180,1); s->queued_angle=add(s->queued_angle,2048);
    if (!s->count_lock) s->completed180=add(s->completed180,1);
   }
  }
 } else s->queued180=0;
 if(s->queued180) { s->yaw_rate=0; return 1; }
 s->turning=0;
 step=asr(mul(in->dt8,(in->global40 || in->global60) ? 409600 : 40960),8);
 left=in->held80 || in->held40; right=in->held90 || in->held60;
 if (left || right) {
  if(s->no_input_since>0 && sub(in->tick,s->no_input_since)<8) left=right=0;
 } else s->no_input_since=in->tick;
 left|=in->held40; right|=in->held60;
 if(s->input_lock)left=right=0;
 if(!left && !right && !in->analog_x)s->yaw_rate=0;
 s->strong_decay=0;
 if(in->analog_x && !left && !right) {
  target=mul(in->analog_x,limit)/128;
  if(target < -limit)target=-limit;
  if(target > limit)target=limit;
  if(s->yaw_rate<target) {
   s->turning=1;
   rate=add(step,mul(step,asr(sub(target,s->yaw_rate),12))/80);
   s->yaw_rate=add(s->yaw_rate,rate);
   if(s->yaw_rate>target)s->yaw_rate=target;
   s->lean_rate=add(s->lean_rate,step);
   if(s->lean_rate>target)s->lean_rate=target;
  }
  if(s->yaw_rate>target) {
   s->turning=1;
   rate=add(step,mul(step,asr(sub(s->yaw_rate,target),12))/80);
   s->yaw_rate=sub(s->yaw_rate,rate);
   if(s->yaw_rate<target)s->yaw_rate=target;
   s->lean_rate=sub(s->lean_rate,step);
   if(s->lean_rate<target)s->lean_rate=target;
  }
 } else if(left) {
  s->turning=1; s->yaw_rate=sub(s->yaw_rate,step);
  if(s->yaw_rate < -limit)s->yaw_rate=-limit;
  rate=sub(s->lean_rate,step); if(rate>=-limit)s->lean_rate=rate;
 } else if(right) {
  s->turning=1; s->yaw_rate=add(s->yaw_rate,step);
  if(s->yaw_rate>limit)s->yaw_rate=limit;
  rate=add(s->lean_rate,step); if(rate<=limit)s->lean_rate=rate;
 } else {
  s->yaw_rate=sub(s->yaw_rate,asr(s->yaw_rate,2));
  if(!asr(add(s->yaw_rate,2048),12))s->yaw_rate=0;
  rate=asr(s->lean_rate,4);
  if(rate<0 && rate>=-1023)rate=-1024;
  if(rate>0 && rate<1024)rate=1024;
  s->lean_rate=sub(s->lean_rate,rate);
  if(!asr(add(s->lean_rate,2048),12))s->lean_rate=0;
 }
 s->lean_rate=s->yaw_rate;
 if(s->yaw_rate && (s->animation==0 || (s->animation>=6 && s->animation<=10))) {
  rate=s->yaw_rate;
  if(!s->crouched) {
   target=mul(rate,rate<0 ? -22 : 22)/limit;
   if(target>22)target=22;
   id=((rate<0) != !!(s->flags&2)) ? 6 : 7;
   frame=s->animation==id ? s->frame : 0;
   /* One original positive/switched branch increments once before easing. */
   if((s->flags&2) && rate>=0 && frame<target)frame=halfword(add(frame,1));
  } else {
   id=((rate<0) != !!(s->flags&2)) ? 9 : 10;
   scale=id==9 ? 15 : 12;
   target=mul(rate,rate<0 ? -scale : scale)/limit;
   if(target>scale)target=scale;
   frame=s->animation==id ? s->frame : 0;
   if(s->animation==(id==9 ? 6 : 7))frame=target;
  }
  frame=approach_frame(frame,target);
  play(s,o,id,frame,frame,-1);
 } else if(!s->yaw_rate) {
  if(s->animation==6 || s->animation==7)play(s,o,0,0,-1,-1);
  else if(s->animation==9 || s->animation==10)play(s,o,8,19,26,19);
 }
 s->pitch_delta=0;
 if(s->state==1 && !in->held10 && !in->held20) {
  step=asr(mul(in->dt8,20),8);
  if(in->helda0 || in->analog_y < -40)s->pitch_delta=sub(0,step);
  if(in->heldb0 || in->analog_y > 40)s->pitch_delta=step;
 }
 return 1;
}
/* Air-physics accounting immediately after its matrix yaw at0x80054658. */
int32_t thps1_air_spin_accumulate_yaw(Thps1AirSpin *s) {
 int32_t delta=asr(s->yaw_rate,12);
 if (!delta) return 0;
 s->queued_angle=add(s->queued_angle,delta);
 if(s->queued_angle>=2048) {
  s->queued_angle=sub(s->queued_angle,2048);
  if(!s->count_lock)s->completed180=add(s->completed180,1);
 }
 if(s->queued_angle<=-2048) {
  s->queued_angle=add(s->queued_angle,2048);
  if(!s->count_lock)s->completed180=add(s->completed180,1);
 }
 return delta;
}
static float fmul(float a,float b) { volatile float v=a*b; return v; }
static float fadd(float a,float b) { volatile float v=a+b; return v; }
void thps1_air_spin_rotate_basis_q12(int16_t basis[9],const int16_t rotation[9]) {
 int16_t result[9]; int row,col,k;
 for(row=0;row<3;++row)for(col=0;col<3;++col) {
  float terms[3],value;
  for(k=0;k<3;++k)terms[k]=fmul(fmul((float)basis[row*3+k],1.0f/4096.0f),fmul((float)rotation[k*3+col],1.0f/4096.0f));
  value=fmul(fadd(fadd(terms[0],terms[1]),terms[2]),4096.0f);
  result[row*3+col]=(int16_t)halfword((int32_t)value);
 }
 memcpy(basis,result,sizeof(result));
}
void thps1_air_spin_pitch_basis_q12(int16_t basis[9],int32_t position[3],const int16_t rotation[9]) {
 int i; int16_t old_up[3];
 for(i=0;i<3;++i)old_up[i]=basis[i*3+1];
 thps1_air_spin_rotate_basis_q12(basis,rotation);
 for(i=0;i<3;++i)position[i]=add(position[i],70*((int32_t)old_up[i]-basis[i*3+1]));
}
