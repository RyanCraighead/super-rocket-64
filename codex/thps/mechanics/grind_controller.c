/* Source ranges and explicitly excluded engine boundaries: grind_evidence.md. */
#include "grind_controller.h"
#include "ground_kernels.h"
#include <limits.h>
#include <math.h>
#include <string.h>
extern int32_t thps1_ollie_charge_limit(int32_t,int32_t);
extern int32_t thps1_ollie_impulse(int32_t,int32_t,int32_t,int);
static int32_t word(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n;}
static int32_t add(int32_t a,int32_t b){return word((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return word((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return word((uint32_t)a*(uint32_t)b);}
static int32_t shr(int32_t a,unsigned n){return a>=0?(int32_t)((uint32_t)a>>n):-1-(int32_t)(~(uint32_t)a>>n);}
static int32_t neg(int32_t a){return sub(0,a);}
static int32_t mag(int32_t a){return a<0?neg(a):a;}
static float fp(float x){volatile float y=x;return y;}
int32_t thps1_grind_multiply(int32_t a,int32_t b){return (int32_t)fp(fp((float)a*fp((float)b*(1.0f/16777216.0f)))*4096.0f);}
int32_t thps1_grind_dot(const int32_t a[3],const int32_t b[3]){
 float v[3];int i;for(i=0;i<3;i++)v[i]=fp(fp((float)a[i]*(1.0f/4096.0f))*fp((float)b[i]*(1.0f/4096.0f)));
 return (int32_t)fp(fp(fp(v[0]+v[1])+v[2])*4096.0f);
}
int thps1_grind_normalize(int32_t out[3],const int32_t in[3]){
 float v[3],n,r;int i;for(i=0;i<3;i++)v[i]=fp((float)in[i]*(1.0f/4096.0f));
 n=sqrtf(fp(fp(fp(v[0]*v[0])+fp(v[1]*v[1]))+fp(v[2]*v[2])));
 if(n==0)return 0;
 r=fp(1.0f/n);for(i=0;i<3;i++)out[i]=(int32_t)fp(fp(v[i]*r)*4096.0f);return 1;
}
void thps1_grind_rng_seed(Thps1GrindRng *r,uint32_t seed){r->a=seed;r->b=0x12b9b0a1u;r->c=0x0aa2fb3fu;}
int32_t thps1_grind_random(Thps1GrindRng *r,int32_t bound){
 r->a=r->a*r->b+r->c;
 r->b=(r->b^r->a)+(uint32_t)shr(word(r->a),4);
 r->c=r->c+0xefefeff0u+(uint32_t)shr(word(r->a),3);
 return shr(mul((int32_t)(r->a&65535u),bound),16);
}
void thps1_grind_reset(Thps1GrindState *s,uint32_t seed){
 memset(s,0,sizeof(*s));s->rail_id=s->chain_id=s->last_chain_id=-1;
 s->speed_stat=7;s->balance_stat=4;s->ollie_stat=3;s->air_stat=7;
 s->animation.rate=65536;thps1_grind_rng_seed(&s->rng,seed);
}
void thps1_grind_ground_reset(Thps1GrindState *s){s->balance=s->balance_velocity=s->balance_ticks=0;}
int32_t thps1_grind_low_speed(int32_t speed,int32_t y){
 if(!speed)speed=1000;
 if(mag(speed)>59999)return speed;
 if(y>0)return add(speed,2000);
 if(y<0)return add(speed,-2000);
 return mul(speed,5)/4;
}
int32_t thps1_grind_balance_position(int32_t p,int32_t v,int32_t ticks,int32_t stat){
 int32_t time=add(ticks,60);p=add(p,mul(p/add(mul(stat,3),28),time)/80);
 return add(p,mul(v,time)/80);
}
void thps1_grind_balance_step(Thps1GrindState *s,const Thps1GrindInput *in){
 s->balance=thps1_grind_balance_position(s->balance,s->balance_velocity,s->balance_ticks,s->balance_stat);
 if(!in->left&&in->x>=-40&&!in->right&&in->x<=40)s->balance_enabled=1;
 if((in->left||in->x< -40)&&s->balance_enabled)s->balance_velocity=add(s->balance_velocity,-20);
 else if((in->right||in->x>40)&&s->balance_enabled)s->balance_velocity=add(s->balance_velocity,20);
 else if(mag(s->balance_velocity)<20-s->balance_stat){
  int32_t bound=80-3*s->balance_stat;
  s->balance_velocity=thps1_grind_random(&s->rng,bound)-bound/2;
 }
 if(mag(s->balance)>4000){s->events|=THPS1_GRIND_BAIL;s->active=0;}
 s->tilt_q12=s->balance/10;
 if(s->stance==1)s->tilt_q12=neg(s->tilt_q12);
 s->tilt_q12&=4095;
}
int thps1_grind_select_trick(int32_t d,int sw,const Thps1GrindInput *in,int32_t *turn){
 int t=6;*turn=0;
 if(mag(d)>=2867){if(sw)d=neg(d);return d<0?2:3;}
 if(in->down||in->y>40){t=7;if(in->left||in->x< -40)*turn=-180;else if(in->right||in->x>40)*turn=180;else t=4;}
 if(in->up||in->y< -40){t=8;if(!in->left&&in->x>=-40&&!in->right&&in->x<=40)t=5;}
 return t;
}
void thps1_grind_animation_start(Thps1GrindState *s,const ThpsAnimBank *b){
 static const int clips[7]={17,17,67,68,70,58,57};
 if(s->trick<2||s->trick>8)return;
 s->stance=s->trick==2?2:s->trick==3?1:0;
 thps1_anim_run(&s->animation,b,clips[s->trick-2],s->trick==2?1:0,s->trick==2?8:-1,-1);
}
void thps1_grind_animation_continue(Thps1GrindState *s,const ThpsAnimBank *b,int32_t frame){
 int clip=s->animation.id,from=0;
 if(!s->animation.finished)return;
 if(clip==57||clip==58){int32_t x=(frame/4)%4;if(x>2)x=4-x;s->animation.frame=(int16_t)(x+(clip==57?8:6));return;}
 if(clip==17)clip=s->animation.frame>=16?39:37;
 else if(clip==37)clip=38;
 else if(clip==67||clip==40){clip=40;if(s->profile_id!=0&&s->profile_id!=0x6000)from=3;}
 else if(clip==68||clip==41){clip=41;if(s->profile_id!=0&&s->profile_id!=0x6000)from=5;}
 else if(clip==70)clip=42;
 thps1_anim_run(&s->animation,b,clip,from,-1,-1);
}
static int bounded(const int32_t v[3],int32_t bound){int i;for(i=0;i<3;i++)if(v[i]<-bound||v[i]>bound)return 0;return 1;}
static int valid_bank(const ThpsAnimBank *b){uint32_t i;if(!b||!b->frame_counts||b->clip_count!=78)return 0;for(i=0;i<78;i++)if(!b->frame_counts[i]||b->frame_counts[i]>127)return 0;return 1;}
static int direction(const Thps1GrindRail *r,int32_t d[3]){int i;if(!bounded(r->a,1000000000)||!bounded(r->b,1000000000))return 0;for(i=0;i<3;i++)d[i]=sub(r->b[i],r->a[i]);return thps1_grind_normalize(d,d);}
int thps1_grind_enter(Thps1GrindState *s,const Thps1GrindEntry *e,const Thps1GrindInput *in,const ThpsAnimBank *bank){
 Thps1GrindState n;int32_t d[3],dp;int i,forward;
 if(!s||!e||!in||!valid_bank(bank)||!direction(&e->rail,d)||!bounded(e->velocity,1000000)||!bounded(e->right,4096)||!bounded(e->closest,1000000000))return -1;
 if(s->balance_stat<0||s->balance_stat>20||s->speed_stat<0||s->speed_stat>30)return -1;
 if(!in->grind||e->bailed||e->input_locked)return 0;
 if(e->source_state==2&&e->velocity[1]>0)return 0;
 if(s->cooldown>0){s->cooldown--;if(s->cooldown>0)return 0;}
 if(s->active||!e->candidate_valid||!e->ready)return 0;
 if(e->distance<0||e->distance>=100)return 0;
 if((e->source_state==0||e->source_state==2)&&e->closest[1]<e->actor_position[1])return 0;
 dp=thps1_grind_dot(d,e->velocity);forward=dp>=0;
 if(!memcmp(e->closest,forward?e->rail.b:e->rail.a,sizeof(e->closest)))return 0;
 n=*s;n.events=THPS1_GRIND_ENTER;n.active=1;n.balance_enabled=0;n.cooldown=0;
 n.rail_id=e->rail.id;n.chain_id=e->rail.chain_id;
 for(i=0;i<3;i++){n.position[i]=e->closest[i];n.travel_direction[i]=forward?d[i]:neg(d[i]);n.velocity[i]=add(e->velocity[i],mul(n.travel_direction[i],n.speed_stat+5));n.acceleration[i]=0;}
 n.position[1]=add(n.position[1],-102400);
 if(n.chain_id!=n.last_chain_id){n.balance_ticks=add(n.balance_ticks,-30);if(n.balance_ticks<0)n.balance_ticks=0;n.last_chain_id=n.chain_id;n.balance_velocity=0;}
 else n.balance_ticks=add(n.balance_ticks,40);
 n.balance_velocity=mul(mul(n.balance_velocity,100-15*n.balance_stat)/100,add(n.balance_ticks,60))/80;
 n.trick=(uint8_t)thps1_grind_select_trick(thps1_grind_dot(n.travel_direction,e->right),e->switch_stance,in,&n.turn_q12);
 thps1_grind_animation_start(&n,bank);n.previous_ollie=in->ollie;*s=n;return 1;
}
static void release(Thps1GrindState *s,const ThpsAnimBank *bank){
 s->active=0;s->cooldown=5;s->events|=THPS1_GRIND_END;s->animation.rate=65536;
 thps1_anim_run(&s->animation,bank,14,0,-1,-1);
}
static void face(Thps1GrindState *s,const int32_t d[3],int32_t speed){
 int i;for(i=0;i<3;i++)s->travel_direction[i]=speed>=0?neg(d[i]):d[i];
 if(s->stance){s->facing[0]=s->travel_direction[2];s->facing[1]=s->travel_direction[1];s->facing[2]=neg(s->travel_direction[0]);}
 else memcpy(s->facing,s->travel_direction,sizeof(s->facing));
 if(s->stance==2)for(i=0;i<3;i++)s->facing[i]=neg(s->facing[i]);
}
int thps1_grind_handle_jump(Thps1GrindState *s,const Thps1GrindInput *in,const ThpsAnimBank *bank){
 Thps1GrindState n;int i;
 if(!s||!in||!s->active||!valid_bank(bank)||!bounded(s->velocity,1000000)||!bounded(s->travel_direction,4096)||s->ollie_stat<0||s->ollie_stat>10||s->air_stat<0||s->air_stat>10)return -1;
 n=*s;n.events=0;n.dismount_turn_q12=0;
 if(in->ollie){int32_t limit=thps1_ollie_charge_limit(n.air_stat,n.ollie_stat);n.charge_ticks=add(n.charge_ticks,1);if(n.charge_ticks>limit)n.charge_ticks=limit;}
 else if(n.previous_ollie){
  int32_t dot=thps1_grind_dot(n.travel_direction,n.velocity);
  for(i=0;i<3;i++)n.velocity[i]=thps1_grind_multiply(n.travel_direction[i],dot);
  n.velocity[1]=sub(n.velocity[1],thps1_ollie_impulse(n.air_stat,n.ollie_stat,n.charge_ticks,0));
  n.dismount_turn_q12=in->right?200:in->left?-200:0;n.cooldown=5;n.active=0;n.events=THPS1_GRIND_OLLIE;n.charge_ticks=0;
  n.animation.rate=65536;thps1_anim_run(&n.animation,bank,4,0,-1,-1);n.previous_ollie=0;*s=n;return 1;
 }
 n.previous_ollie=in->ollie;*s=n;return 0;
}
int thps1_grind_motion_pre_final(Thps1GrindState *s,const Thps1GrindRail *rail,const Thps1GrindInput *in,const ThpsAnimBank *bank,int32_t frame,Thps1GrindRailLookup lookup,void *ctx){
 Thps1GrindState n;Thps1GrindRail r;int32_t d[3],accdot,speed,len,along;int i,axis,first=1,links=0;
 if(!s||!rail||!in||!s->active||s->rail_id!=rail->id||!valid_bank(bank)||!bounded(s->velocity,1000000)||!bounded(s->position,1000000000)||!direction(rail,d)||s->balance_stat<0||s->balance_stat>20||s->speed_stat<0||s->speed_stat>30||s->ollie_stat<0||s->ollie_stat>10||s->air_stat<0||s->air_stat>10)return 0;
 n=*s;r=*rail;n.events=0;n.dismount_turn_q12=0;n.balance_ticks=add(n.balance_ticks,1);
thps1_grind_animation_continue(&n,bank,frame);
 for(;;){
  if(!direction(&r,d))return 0;
  speed=thps1_grind_dot(d,n.velocity);
  if(first){speed=thps1_grind_low_speed(speed,d[1]);n.acceleration[0]=0;n.acceleration[1]=13000;n.acceleration[2]=0;first=0;}
  accdot=thps1_grind_dot(d,n.acceleration);
  for(i=0;i<3;i++){n.acceleration[i]=thps1_grind_multiply(d[i],accdot);n.velocity[i]=thps1_grind_multiply(d[i],speed);}
  axis=0;if(mag(d[0])<mag(d[1]))axis=1;if(mag(d[axis])<mag(d[2]))axis=2;
  len=mul(sub(r.b[axis],r.a[axis])/d[axis],4096);
  along=add(mul(sub(n.position[axis],r.a[axis])/d[axis],4096),speed);
  if(along>=0&&along<=len){for(i=0;i<3;i++)n.position[i]=add(n.position[i],add(n.velocity[i],n.acceleration[i]/2));face(&n,d,speed);break;}
  {Thps1GrindRail next;int32_t nd[3];int32_t id=along<0?r.previous_id:r.next_id;
   if(id<0||!lookup||!lookup(ctx,id,&next)){release(&n,bank);face(&n,d,speed);break;}
   if(!direction(&next,nd))return 0;
   if(thps1_grind_dot(d,nd)<2633){release(&n,bank);face(&n,d,speed);break;}
   if(++links>32)return 0; /* host malformed/cyclic rail safety; no gameplay cap */
   for(i=0;i<3;i++)n.position[i]=along<0?r.a[i]:r.b[i];
   n.position[1]=add(n.position[1],-102400);
   r=next;n.rail_id=r.id;n.events|=THPS1_GRIND_LINK;
  }
 }

 thps1_grind_balance_step(&n,in);
 *s=n;return 1;
}

int thps1_grind_step_pre_final(Thps1GrindState *s,const Thps1GrindRail *rail,const Thps1GrindInput *in,const ThpsAnimBank *bank,int32_t frame,Thps1GrindRailLookup lookup,void *ctx){
 Thps1GrindState n;int result;int32_t d[3];
 if(!s||!rail||s->rail_id!=rail->id||!direction(rail,d))return 0;
 n=*s;result=thps1_grind_handle_jump(&n,in,bank);if(result<0)return 0;
 if(result)n.balance_ticks=add(n.balance_ticks,1);
 else if(!thps1_grind_motion_pre_final(&n,rail,in,bank,frame,lookup,ctx))return 0;
 *s=n;return 1;
}

void thps1_grind_finish_velocity(Thps1GrindState *s){
 int i;
 /* Global post-state acceleration integration and speed cap, also used in air. */
 for(i=0;i<3;i++)s->velocity[i]=add(s->velocity[i],s->acceleration[i]);
 {int32_t vdot=thps1_grind_dot(s->velocity,s->velocity);int32_t measured=vdot>0?(int32_t)sqrtf((float)vdot)*64:0;
  int32_t cap=thps1_ground_speed_from_total(s->speed_stat+50);
  if(measured>cap)for(i=0;i<3;i++)s->velocity[i]=thps1_ground_cap_component(s->velocity[i],cap,measured);
 }
}
int thps1_grind_step(Thps1GrindState *s,const Thps1GrindRail *rail,const Thps1GrindInput *in,const ThpsAnimBank *bank,int32_t frame,Thps1GrindRailLookup lookup,void *ctx){
 if(!thps1_grind_step_pre_final(s,rail,in,bank,frame,lookup,ctx))return 0;
 if(!(s->events&THPS1_GRIND_OLLIE))thps1_grind_finish_velocity(s);
 return 1;
}
/* 0x80051e14..e74: cross products use integer low words/SRA12;
 * only side is normalized, then up is reconstructed from side and facing. */
int thps1_grind_basis_q12(int16_t basis[9],const int32_t facing[3]){
 int32_t up[3]={0,-4096,0},side[3],newup[3];int i;
 if(!basis||!facing||!bounded(facing,4096))return 0;
 for(i=0;i<3;i++){int j=(i+1)%3,k=(i+2)%3;side[i]=shr(sub(mul(facing[j],up[k]),mul(facing[k],up[j])),12);}
 if(!thps1_grind_normalize(side,side))return 0;
 for(i=0;i<3;i++){int j=(i+1)%3,k=(i+2)%3;newup[i]=shr(sub(mul(side[j],facing[k]),mul(side[k],facing[j])),12);}
 for(i=0;i<3;i++){basis[3*i]=(int16_t)side[i];basis[3*i+1]=(int16_t)newup[i];basis[3*i+2]=(int16_t)facing[i];}
 return 1;
}
void thps1_grind_roll_from_pitch_q12(int16_t roll[9],const int16_t pitch[9]){
 int16_t p[9];memcpy(p,pitch,sizeof(p));memset(roll,0,9*sizeof(*roll));
 roll[0]=p[4];roll[1]=p[5];roll[3]=p[7];roll[4]=p[8];roll[8]=p[0];
}
/* Velocity part of source58594,58620..587dc. Caller supplies the prior source
 * basis864, and the original22598 yaw matrix. Apply BEFORE the ollie impulse.
 * Source collision-normal equality gate must already have passed. */
void thps1_grind_turn_velocity_q12(int32_t velocity[3],const int16_t previous[9],const int16_t yaw[9]){
 int16_t transposed[9],matrix[9],tmp[9];int32_t row[3],result[3],before,after;int i,j,k;
 before=(int32_t)sqrtf((float)(uint32_t)thps1_grind_dot(velocity,velocity))*64;
 for(i=0;i<3;i++)for(j=0;j<3;j++)transposed[i*3+j]=previous[j*3+i];
 memcpy(matrix,previous,sizeof(matrix));
 /* Same float32 matrix product as source22fcc, without another module owner. */
 for(k=0;k<2;k++){
  const int16_t *a=k?matrix:transposed,*b=k?transposed:yaw;
  for(i=0;i<3;i++)for(j=0;j<3;j++){
   float x=fp(fp((float)a[i*3]/4096.0f)*fp((float)b[j]/4096.0f));
   float y=fp(fp((float)a[i*3+1]/4096.0f)*fp((float)b[3+j]/4096.0f));
   float z=fp(fp((float)a[i*3+2]/4096.0f)*fp((float)b[6+j]/4096.0f));
   int32_t v=(int32_t)fp(fp(fp(x+y)+z)*4096.0f);uint32_t low=(uint32_t)v&65535u;
   tmp[i*3+j]=(int16_t)(low<32768?(int32_t)low:(int32_t)low-65536);
  }
  memcpy(k?matrix:transposed,tmp,sizeof(tmp));
 }
 for(i=0;i<3;i++){for(j=0;j<3;j++)row[j]=matrix[i*3+j];result[i]=thps1_grind_dot(row,velocity);}
 after=(int32_t)sqrtf((float)(uint32_t)thps1_grind_dot(result,result))*64;
 before=shr(before,8);after=shr(after,8);
 for(i=0;i<3;i++){int32_t v=mul(result[i],before);velocity[i]=after?v/after:v;}
}
/* Rail-acquire safety gate0x8004e174..e23c, before normal grind entry. */
int thps1_grind_entry_bail(int32_t active,int32_t interruptible,int32_t frame,int32_t count,uint32_t flags,int32_t landing_frames,int32_t rotation_active,int32_t rotation_angle,int32_t rotation_direction){
 int32_t threshold=(flags&0x6000u)?sub(count,landing_frames):10000;
 if(active&&!interruptible&&frame<threshold)return 1;
 if(rotation_active){int32_t a=(int32_t)((uint32_t)rotation_angle&4095u);return rotation_direction>0?a<3796:a>=301;}
 return 0;
}
