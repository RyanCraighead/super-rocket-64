#include "bm64_bomb_kernel.h"
#include <string.h>
#include "../movement/bm64_math.h"

static int16_t s16(int32_t v) {
 uint32_t u=(uint32_t)v & UINT32_C(65535);
 return (int16_t)(u < 32768 ? (int32_t)u : (int32_t)u-65536);
}
static float f32(uint32_t bits) { float f; memcpy(&f,&bits,sizeof f); return f; }
static float base_step(int16_t type) {
 /* ROM 802a3d94/98/9c; identical to 3ce8/3cf0/3cf4. */
 return f32((type & BM64_BOMB_HUGE) ? 0x3d088889u :
            (type & BM64_BOMB_SHORT_FUSE) ? 0x3c088889u : 0x3b5a740eu);
}
int bm64_bomb_model_id(int16_t type) {
 /* 8027356c + jump table 802a3cc8, low nibble >=4 maps to model0. */
 static const int model[4]={0,2,4,5};
 unsigned n=(unsigned)(uint16_t)type & 15u;
 return n<4 ? model[n] : 0;
}
void bm64_bomb_init(Bm64Bomb *b, int16_t type, int16_t owner, int32_t fire) {
 memset(b,0,sizeof *b);
 b->type=type; b->owner=owner; b->fire_power=s16((int32_t)((uint32_t)fire+1u));
 b->fuse=(type & BM64_BOMB_SHORT_FUSE) ? 60 : 150;
 b->state=(type & BM64_BOMB_REMOTE) ? 0 : BM64_BOMB_FUSE_ACTIVE;
 b->heading=9; b->particle_id=-1; b->radius=40.0f; b->height=40.0f;
 b->scale=b->scale_max=(type & BM64_BOMB_HUGE) ? 10.0f : 1.0f;
 b->scale_min=(type & BM64_BOMB_HUGE) ? 9.0f : f32(0x3f666666u);
 b->scale_step=base_step(type);
}
void bm64_bomb_pump_input(Bm64Bomb *b) {
 /* 80273eec..80273ef8, after the caller resolves an actually held bomb. */
 b->pump_charge=s16((int32_t)b->pump_charge+30);
}
uint32_t bm64_bomb_visual_tick(Bm64Bomb *b, int16_t pause, int blocked) {
 uint32_t events=0;
 if(pause==1) return 0; /* 80276d68..80276d80 */
 if(b->particle_id!=-1) {
  if(b->particle_age>=61) { events|=BM64_BOMB_EVENT_STOP_PARTICLE; b->particle_id=-1; }
  else b->particle_age=s16((int32_t)b->particle_age+1);
 }
 /* Two consecutive blocks: a deflation->inflation transition can execute
  * BOTH blocks in one tick. The double operations are original cvt.d.s,
  * mul.d/add.d/cvt.s.d. Do not replace with float step*=1.1f. */
 if(b->state & BM64_BOMB_DEFLATING) {
  float step=b->scale_step;
  b->scale=b->scale-step;
  if(b->scale < b->scale_min) {
   b->state=(int16_t)(b->state & ~BM64_BOMB_DEFLATING);
   b->scale_step=base_step(b->type);
  } else b->scale_step=(float)((double)step+(double)step*0.1);
 }
 if(!(b->state & BM64_BOMB_DEFLATING)) {
  b->scale=b->scale+b->scale_step;
  if(b->fuse<15) {
   float limit=(b->type & BM64_BOMB_HUGE) ? 13.0f : b->scale_max;
   if(b->scale>=limit) b->state=(int16_t)(b->state|BM64_BOMB_SCALE_LIMIT);
  }
  if(b->scale>b->scale_max) {
   b->state=(int16_t)(b->state|BM64_BOMB_DEFLATING);
   b->scale_step=base_step(b->type);
  } else {
   double step=(double)b->scale_step;
   b->scale_step=(float)(step+step*0.1);
  }
 }
 b->pump_charge=s16((int32_t)b->pump_charge-1);
 if(b->pump_charge<0) b->pump_charge=0;
 if(b->pump_sound_timer1>0) b->pump_sound_timer1=s16((int32_t)b->pump_sound_timer1-1);
 if(b->pump_sound_timer2>0) b->pump_sound_timer2=s16((int32_t)b->pump_sound_timer2-1);
 if((b->type & BM64_BOMB_HUGE) || (b->state & BM64_BOMB_PUMPED)) return events;
 b->scale_max=1.0f; b->scale_min=f32(0x3f666666u);
 if(b->pump_charge<100) b->pump_stage=0;
 if(b->pump_charge>=101 && b->pump_charge<=200) {
  if(b->pump_stage<=0 && b->pump_sound_timer1<=0) {
   events|=BM64_BOMB_EVENT_SOUND_16; b->pump_sound_timer1=60;
  }
  b->pump_stage=1; b->scale_max=f32(0x3fa66666u); b->scale_min=f32(0x3f8ccccdu);
 }
 if(b->pump_charge>=201) {
  if(b->pump_stage<2 && b->pump_sound_timer2<=0) {
   events|=BM64_BOMB_EVENT_SOUND_17; b->pump_sound_timer2=60;
  }
  b->pump_stage=2; b->scale_max=f32(0x3fcccccdu); b->scale_min=f32(0x3fb33333u);
 }
 if(b->pump_charge>=301) {
  b->radius=70.0f; b->height=70.0f;
  if(blocked) { b->radius=40.0f; b->height=40.0f; }
  else {
   b->scale_max=f32(0x3ff33333u); b->scale_min=f32(0x3fd9999au);
   b->type=(int16_t)(b->type|BM64_BOMB_FULL_POWER);
   b->state=(int16_t)(b->state|BM64_BOMB_PUMPED);
   b->particle_age=0;
   /* Caller stores particle allocator result in particle_id. */
   events|=BM64_BOMB_EVENT_SOUND_1A|BM64_BOMB_EVENT_PUMP_PARTICLE|BM64_BOMB_EVENT_FULL_PUMP;
  }
 }
 return events;
}
void bm64_bomb_fuse_tick(Bm64Bomb *b, int16_t pause) {
 /* 80276584..80276618. No premature timer<=0 explosion simplification. */
 if(pause==1) return;
 if(b->state & BM64_BOMB_FUSE_ACTIVE) b->fuse=s16((int32_t)b->fuse-1);
 if(b->fuse<15) b->scale_max=(b->type & BM64_BOMB_HUGE) ? 13.0f :
            (b->state & BM64_BOMB_PUMPED) ? 2.0f : f32(0x3fa66666u);
}
int bm64_bomb_commit_scale_explosion(Bm64Bomb *b) {
 /* 80276c28..80276c70, called after host motion/collision. */
 if(b->state & BM64_BOMB_SCALE_LIMIT) b->state=(int16_t)(b->state|BM64_BOMB_EXPLODING);
 return (b->state & BM64_BOMB_EXPLODING)!=0;
}
int bm64_bomb_pickup(Bm64Bomb *b) {
 /* 80271330..80271424, engine detachment calls omitted explicitly. */
 if(b->state & BM64_BOMB_PUMPED) return 0;
 if(b->state & BM64_BOMB_ROLLING) bm64_bomb_stop_rolling(b);
 b->state=(int16_t)((b->state & ~(BM64_BOMB_AIRBORNE|BM64_BOMB_GROUNDED|BM64_BOMB_FUSE_ACTIVE))|BM64_BOMB_HELD);
 b->fuse=(b->type & BM64_BOMB_SHORT_FUSE) ? 60 : 150;
 b->scale_max=1.0f;
 return 1;
}
void bm64_bomb_release(Bm64Bomb *b) {
 /* 80270474..802704e8. Pointer/render/collision side effects are host-owned. */
 b->state=(int16_t)((b->state & ~BM64_BOMB_HELD)|BM64_BOMB_AIRBORNE);
 if(!(b->type & BM64_BOMB_REMOTE)) b->state=(int16_t)(b->state|BM64_BOMB_FUSE_ACTIVE);
 b->fuse=(b->type & BM64_BOMB_SHORT_FUSE) ? 60 : 150;
}
int bm64_bomb_detonate_first(Bm64Bomb *b,size_t count,int16_t owner,int enabled) {
 size_t i;
 if(!enabled) return -1;
 for(i=0;i<count;i++) if(b[i].owner==owner && (b[i].type & BM64_BOMB_REMOTE)
   && !(b[i].state & BM64_BOMB_HELD) && b[i].position[0]!=10000000.0f) {
  b[i].state=(int16_t)(b[i].state|BM64_BOMB_EXPLODING); return (int)i;
 }
 return -1;
}
int bm64_bomb_stop_rolling(Bm64Bomb *b) {
 if(!(b->state & BM64_BOMB_ROLLING)) return 0;
 b->heading=9; b->state=(int16_t)(b->state & ~BM64_BOMB_ROLLING); return 1;
}
int bm64_bomb_throw_preset(int16_t mode,Bm64BombThrowPreset *p) {
 static const float planar[4]={0.0f,8.25f,11.0f,14.0f};
 if(mode<0 || mode>3) return 0;
 p->lift=mode==0 ? 0.0f : 25.0f;
 p->downward_velocity=1.0f; p->planar_speed=planar[mode]; p->gravity=f32(0x3fd11112u);
 return 1;
}
int bm64_bomb_heading_from_degrees(float a) {
 /* 802704f8..80270710. Only ONE wrap; upper cutoffs are integers, not22.5. */
 if(a>=360.0f) a=a-360.0f;
 else if(a<0.0f) a=a+360.0f;
 if(a>337.0f || a<=22.0f) return 4;
 if(a>22.0f && a<=67.0f) return 3;
 if(a>67.0f && a<=112.0f) return 2;
 if(a>112.0f && a<=157.0f) return 1;
 if(a>157.0f && a<=202.0f) return 0;
 if(a>202.0f && a<=247.0f) return 7;
 if(a>247.0f && a<=292.0f) return 6;
 if(a>292.0f && a<=337.0f) return 5;
 return 4;
}

int bm64_explosion_model_id(int16_t component,int16_t type) {
 static const int normal[9]={0x10,0x11,0x06,0x08,0x08,0x0b,0x0d,0x0e,0x0f};
 if(component<0 || component>8) return -1;
 if(type&5) { if(component<=2) return 7; if(component<=4) return 9; }
 return normal[component];
}
int bm64_explosion_init(Bm64Explosion *e,int16_t component,int16_t type,
                        int16_t fire,int16_t delay) {
 static const int duration[7]={20,24,28,32,36,40,45};
 static const float amplitude[7]={3.0f,4.5f,6.0f,7.5f,9.0f,10.5f,12.0f};
 int d=(fire>=3 && fire<=9) ? duration[fire-3] : 40;
 if(component<0 || component>8) return 0;
 memset(e,0,sizeof *e);
 e->component=component; e->delay=delay; e->alive=1; e->opacity=255; /*80230e30*/
 e->angle_step=component==5 ? 4 : component==8 ? (int16_t)(90.0f/((float)d*0.5f)) : (int16_t)(90/d);
 e->damaging=component<=2;
 e->scale=component==5 ? f32(0x3d4ccccdu) : f32(0x3dcccccdu);
 if(type&5) { if(fire>=10) fire=8; }
 else if(fire>=7) fire=5;
 if(type&0x100) fire=(type&5) ? 9 : 6;
 e->fire_level=fire;
 e->amplitude=(fire>=3 && fire<=9) ? amplitude[fire-3] : 3.0f;
 return 1;
}
void bm64_explosion_tick(Bm64Explosion *e,float sn,float cs) {
 float scaled;
 if(!e->alive) return;
 if(e->delay>0) { e->delay=s16((int32_t)e->delay-1); return; }
 e->angle=s16((int32_t)e->angle+e->angle_step);
 /* 80278868..80278c6c: separate f32 multiplies/adds are intentional. */
 if(e->component<=1) {
  float a=cs*191.0f; a=a+64.0f; e->opacity=s16((int32_t)a);
 } else { float a=cs*255.0f; e->opacity=s16((int32_t)a); }
 if(e->component!=5) e->rotation=e->rotation+5.0f;
 scaled=e->amplitude*sn;
 if(e->component==4) scaled=scaled*f32(0x3f333333u);
 else if(e->component==7) scaled=scaled*1.5f;
 else if(e->component==8) scaled=scaled*f32(0x3f266666u);
 e->scale=scaled;
 if(e->rotation>=360.0f) e->rotation=e->rotation-360.0f;
 else if(e->rotation<0.0f) e->rotation=e->rotation+360.0f;
 if(e->angle>=71) e->damaging=0;
 if(e->angle>=91) e->alive=0;
}
float bm64_explosion_hit_radius(const Bm64Explosion *e) {
 /* Shape1 radius40 in8027845c..74, uniform scale from80278c70..9c.
  * Effective sphere extraction8022e26c must retain host actor-shape adapter. */
 return e->scale*40.0f;
}

void bm64_explosion_tick_original(Bm64Explosion *e) {
 float angle=(float)s16((int32_t)e->angle+e->angle_step);
 bm64_explosion_tick(e,bm64_sin_degrees(angle),bm64_cos_degrees(angle));
}

void bm64_bomb_flat_delta(int16_t heading,float speed,float *dx,float *dz) {
 static const int8_t x[10]={0,1,1,1,0,-1,-1,-1,0,0};
 static const int8_t z[10]={-1,-1,0,1,1,1,0,-1,0,0};
 if(heading<0 || heading>9) { *dx=0; *dz=0; return; }
 if(heading<8 && (heading&1)) speed=(float)((double)speed*0.707);
 *dx=0.0f+(float)x[heading]*speed; *dz=0.0f+(float)z[heading]*speed;
}
static float near_zero(float v) {
 const float epsilon=f32(0x38d1b717u); /* ROM8029fe30 */
 return v<=epsilon && v>=-epsilon ? 0.0f : v;
}
void bm64_hold_begin(Bm64HoldTween *h,const float player[3],const float bomb[3],
                     float facing,float player_radius,float player_height,float bomb_radius) {
 float sn=bm64_sin_degrees(facing),cs=bm64_cos_degrees(facing);
 float bx=near_zero(bomb_radius*sn),bz=near_zero(bomb_radius*cs);
 float px=near_zero(player_radius*sn),pz=near_zero(player_radius*cs);
 h->timer=20;
 h->residual_x=(player[0]+bx)-bomb[0];
 h->residual_z=(player[2]+bz)-bomb[2];
 h->phase=0.0f;h->previous_wave=0.0f;
 h->lift_distance=(player_height+player[1])-bomb[1];
 h->local[0]=px-h->residual_x;
 h->local[1]=bomb[1]-player[1];
 h->local[2]=pz-h->residual_z;
}
void bm64_hold_tick(Bm64HoldTween *h,float facing,float player_height,float bomb_radius) {
 float bx,bz,wave;
 if(h->timer<=0) return;
 bx=near_zero(bomb_radius*bm64_sin_degrees(facing));
 bz=near_zero(bomb_radius*bm64_cos_degrees(facing));
 h->local[0]=bx-h->residual_x;
 h->residual_x=h->residual_x-(h->residual_x/(float)h->timer);
 h->local[2]=bz-h->residual_z;
 h->residual_z=h->residual_z-(h->residual_z/(float)h->timer);
 wave=bm64_sin_degrees(h->phase)*h->lift_distance;
 h->local[1]=h->local[1]+(wave-h->previous_wave);
 h->timer=s16((int32_t)h->timer-1);
 h->previous_wave=wave;h->phase=h->phase+4.5f;
 if(h->timer<=0) {h->local[0]=0.0f;h->local[1]=player_height;h->local[2]=bomb_radius;}
}
