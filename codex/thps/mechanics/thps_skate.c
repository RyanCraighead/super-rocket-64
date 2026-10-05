/* Original THPS1 USA Rev1 source arithmetic/state gates in an SM64 contact wrapper.
 * See PROVENANCE.md for exact source-PC evidence and remaining host boundaries.
 * Recovered roll/ollie, ordinary air tricks/spin, landing/bail and rail slice.
 * Host collision, rail records and render anchors remain explicit boundaries.
 */
#include "thps_skate.h"
#include "ground_kernels.h"
#include "ground_kick.h"
#include "air_animation.h"
#include "landing_checks.h"
#include "score_motion.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int32_t signed_word(uint32_t n) { return n<=INT32_MAX?(int32_t)n:-1-(int32_t)~n; }
static int valid_float(float f) { return isfinite(f); }
static int32_t effective_stat(const ThpsSkateState *s,int32_t base) {
 /* Original active-special34c contributes3 to all four equipment/stat sums. */
 return base+(s->scoring.special?3:0);
}
static void sync_score(ThpsSkateState *s) {
 s->score=thps1_score_total(&s->scoring);
 s->combo_score=thps1_score_preview(&s->scoring,s->spin.completed180);
}
static void begin_score(ThpsSkateState *s) {
 thps1_score_begin(&s->scoring);
 thps1_air_tricks_begin_sequence(&s->tricks,signed_word(s->ticks));
 s->spin.queued180=0;s->spin.input_lock=s->spin.count_lock=0;
}
static int32_t grind_points(int32_t trick) {
 /* Original unsigned16 table800d6f7c, ordinary records2..8. */
 static const int32_t base[7]={100,100,125,125,100,150,150};
 return trick>=2&&trick<=8?base[trick-2]:0;
}
static int32_t fixed_mul(int32_t a,int32_t b) {
 /* Original0x80022e14: preserve the three float32 roundings. */
 volatile float x=(float)b*0.000000059604644775390625f;
 volatile float y=(float)a*x;
 volatile float z=y*4096.0f;
 return (int32_t)z;
}
static int32_t fixed_dot(const int32_t a[3],const int32_t b[3]) {
 /* Original0x80022e54; intermediates deliberately float32, no FMA. */
 volatile float x=(float)a[0]/4096.0f*((float)b[0]/4096.0f);
 volatile float y=(float)a[1]/4096.0f*((float)b[1]/4096.0f);
 volatile float z=(float)a[2]/4096.0f*((float)b[2]/4096.0f);
 volatile float xy=x+y,xyz=xy+z,scaled=xyz*4096.0f;
 return (int32_t)scaled;
}
static int32_t source_speed(const int32_t v[3]) {
 /* 0x80022e54 then0x800233b8 then sll6, as in HandleKick. */
 int32_t d=fixed_dot(v,v);
 return d>0?((int32_t)sqrtf((float)d))*64:0;
}
static void get_anim(const ThpsSkateState *s,ThpsAnim *a) {
 memset(a,0,sizeof(*a));a->frame=s->anim_frame_i;a->id=s->animation;
 a->direction=s->anim_direction;a->target=s->anim_target;
 a->continuation=s->anim_continuation;a->fraction=s->anim_fraction;
 a->count=(uint8_t)s->clip_counts[s->animation];a->finished=s->anim_finished;a->rate=s->anim_rate;
}
static void set_anim(ThpsSkateState *s,const ThpsAnim *a) {
 s->anim_frame_i=a->frame;s->animation=a->id;s->anim_direction=a->direction;
 s->anim_target=a->target;s->anim_continuation=a->continuation;
 s->anim_fraction=a->fraction;s->anim_finished=a->finished;s->anim_rate=a->rate;
 /* Renderer samples original discrete poses, no unproven interpolation. */
 s->animation_frame=(float)a->frame;
}
static void finish_orientation(ThpsSkateState *s);
static void anim_run(ThpsSkateState *s,int clip,int from,int to,int continuation,int rate) {
 uint8_t counts[78];ThpsAnim a;ThpsAnimBank bank;unsigned i;
 for(i=0;i<78;i++)counts[i]=(uint8_t)s->clip_counts[i];
 bank.frame_counts=counts;bank.clip_count=78;get_anim(s,&a);a.rate=(uint32_t)rate;finish_orientation(s);
 thps1_anim_run(&a,&bank,clip,from,to,continuation);set_anim(s,&a);
}
static void anim_advance(ThpsSkateState *s) {
 uint8_t counts[78];ThpsAnim a;ThpsAnimBank bank;unsigned i;
 for(i=0;i<78;i++)counts[i]=(uint8_t)s->clip_counts[i];
 bank.frame_counts=counts;bank.clip_count=78;get_anim(s,&a);
 thps1_anim_advance(&a,&bank,256);set_anim(s,&a);
}
/* The original basis came from its polygon collision solver. Here the host supplies
 * a floor normal; reconstructing an orthonormal Q12 board basis is a HOST boundary,
 * not claimed byte-identical to the original collision/matrix pipeline. */
static void host_basis(const ThpsSkateState *s,int32_t f[3],int32_t n[3]) {
 float angle=(float)s->yaw*(6.2831853071795864769f/65536.0f);
 float x=sinf(angle),z=cosf(angle),y=0,len;
 if(s->floor_normal[1]>0.01f)y=-(x*s->floor_normal[0]+z*s->floor_normal[2])/s->floor_normal[1];
 len=sqrtf(x*x+y*y+z*z);x/=len;y/=len;z/=len;
 f[0]=(int32_t)(x*4096);f[1]=(int32_t)(-y*4096);f[2]=(int32_t)(z*4096);
 n[0]=(int32_t)(s->floor_normal[0]*4096);n[1]=(int32_t)(-s->floor_normal[1]*4096);n[2]=(int32_t)(s->floor_normal[2]*4096);
}
static void make_bank(const ThpsSkateState *s,uint8_t counts[78],ThpsAnimBank *bank) {
 unsigned i;for(i=0;i<78;i++)counts[i]=(uint8_t)s->clip_counts[i];
 bank->frame_counts=counts;bank->clip_count=78;
}
static void basis_from_host(ThpsSkateState *s) {
 int32_t f[3],n[3];unsigned i;host_basis(s,f,n);
 for(i=0;i<3;i++){s->source_forward[i]=-f[i];s->source_up[i]=n[i];}
 for(i=0;i<3;i++) {
  unsigned j=(i+1)%3,k=(i+2)%3;
  s->source_side[i]=(s->source_forward[j]*n[k]-s->source_forward[k]*n[j])>>12;
 }
}
static void yaw_from_basis(ThpsSkateState *s) {
 double x=-(double)s->source_forward[0],z=-(double)s->source_forward[2];
 if(x!=0||z!=0) {
  /* This is host heading presentation, not recovered source arithmetic.
   * SM64 exports its own non-libm atan2f; use unambiguous libm atan2 so
   * native linking cannot silently substitute SM64's angle convention. */
  double a=atan2(x,z)*(65536.0/6.2831853071795864769);
  s->yaw=(uint16_t)(int32_t)a;
 }
}
/* Original4d2d4 is an orientation owner, not a new trick or host yaw turn.
 * THE900/McTwist defer their half-turn correction to the next Anim::Run. */
static void apply_orientation(ThpsSkateState *s,uint32_t effects) {
 unsigned i;s->stance_flags=s->tricks.stance_flags;
 if(effects&2u)for(i=0;i<3;i++) {
  s->source_forward[i]=-s->source_forward[i];s->source_side[i]=-s->source_side[i];
 }
 if(effects&2u)yaw_from_basis(s);
}
static void finish_orientation(ThpsSkateState *s) {
 s->tricks.stance_flags=s->stance_flags;
 apply_orientation(s,thps1_air_tricks_finish_orientation(&s->tricks));
}
static int body_from_foot(const ThpsSkateState *s,int32_t body[3]) {
 unsigned i;for(i=0;i<3;i++) {
  double p=(double)s->position[i]/s->source_to_host;
  if(s->body_offset_bound)p+=(double)s->floor_normal[i]*s->body_offset_source;
  p*=4096.0*(i==1?-1:1);
  if(!isfinite(p)||p < -1000000000.0||p > 1000000000.0)return 0;
  body[i]=(int32_t)p;
 }return 1;
}
static void foot_from_body(ThpsSkateState *s) {
 unsigned i;for(i=0;i<3;i++) {
  double p=(double)s->source_body_position[i]/4096.0*(i==1?-1:1);
  p-=(double)s->floor_normal[i]*s->body_offset_source;
  s->position[i]=(float)(p*s->source_to_host);
 }
}
static Thps1GrindInput grind_input(const ThpsSkateInput *in) {
 Thps1GrindInput g;memset(&g,0,sizeof(g));g.x=(int8_t)(in->steer*127);g.y=(int8_t)(-in->forward*127);
 g.left=in->left;g.right=in->right;g.up=in->up;g.down=in->down;g.grind=in->grind;g.ollie=in->ollie;return g;
}
static void clear_trick_queue(Thps1AirTricks *s) {
 s->read_index=s->write_index=s->release_debounce=0;
 memset(s->directions,0,sizeof(s->directions));memset(s->buttons,0,sizeof(s->buttons));
 memset(s->direction_ticks,0,sizeof(s->direction_ticks));memset(s->button_ticks,0,sizeof(s->button_ticks));memset(s->history,0,sizeof(s->history));
}
static void grind_air_history(Thps1AirTricks *s) {
 /* SetState4d060..4d090 clears these history fields, never the input ring. */
 s->history[1]=s->history[2]=s->history[4]=s->history[5]=s->history[6]=s->history[8]=s->history[9]=0;
}
static void start_bail(ThpsSkateState *s,const ThpsAnimBank *bank,int32_t reason,int32_t phase,int32_t clip) {
 finish_orientation(s);get_anim(s,&s->bail.animation);memcpy(s->bail.velocity,s->source_velocity,sizeof(s->source_velocity));
 memcpy(s->bail.acceleration,s->source_acceleration,sizeof(s->source_acceleration));
 s->bail.source_state=s->source_state;s->bail.crouched=s->previous_ollie;s->bail.kick_timer=s->kick_ticks;
 thps1_bail_begin(&s->bail,bank,reason);thps1_bail_override(&s->bail,bank,phase,clip);set_anim(s,&s->bail.animation);
 s->source_state=s->bail.source_state;s->mode=THPS_SKATE_AIR;s->step_new_bail=1;s->events|=THPS_EVENT_BAIL;
 s->charge_ticks=0;s->previous_ollie=0;s->grind.active=0;s->trick_sequence_ready=0;s->jump_latched=s->landing_pending=0;
 thps1_score_bail(&s->scoring);sync_score(s);
 thps1_air_tricks_clear(&s->tricks);memset(&s->spin,0,sizeof(s->spin));s->spin_total=0;
}
static void run_bail(ThpsSkateState *s,const ThpsAnimBank *bank) {
 s->jump_latched=s->landing_pending=0;
 get_anim(s,&s->bail.animation);memcpy(s->bail.velocity,s->source_velocity,sizeof(s->source_velocity));
 memcpy(s->bail.acceleration,s->source_acceleration,sizeof(s->source_acceleration));
 s->bail.source_state=s->source_state;s->bail.kick_timer=s->kick_ticks;
 thps1_bail_step(&s->bail,bank,256,s->step_input.flip);set_anim(s,&s->bail.animation);
 memcpy(s->source_velocity,s->bail.velocity,sizeof(s->source_velocity));memcpy(s->source_acceleration,s->bail.acceleration,sizeof(s->source_acceleration));
 s->kick_ticks=s->bail.kick_timer;
 if(s->bail.events&THPS1_BAIL_RECOVERED){s->events|=THPS_EVENT_RECOVER;s->bailout_ticks=0;}
 else s->bailout_ticks=signed_word((uint32_t)s->bailout_ticks+1u);
}
static int rotate_basis(ThpsSkateState *s,int32_t angle,int pitch,int32_t body[3]) {
 int16_t basis[9];const int16_t *table=pitch?s->pitch_rotations:s->yaw_rotations;unsigned i;
 if(!angle)return 1;
 if(!table)return 0;
 for(i=0;i<3;i++){basis[i*3]=(int16_t)s->source_side[i];basis[i*3+1]=(int16_t)s->source_up[i];basis[i*3+2]=(int16_t)s->source_forward[i];}
 table+=((uint32_t)angle&4095u)*9;
 if(pitch)thps1_air_spin_pitch_basis_q12(basis,body,table);else thps1_air_spin_rotate_basis_q12(basis,table);
 for(i=0;i<3;i++){s->source_side[i]=basis[i*3];s->source_up[i]=basis[i*3+1];s->source_forward[i]=basis[i*3+2];}
 yaw_from_basis(s);return 1;
}
static void pack_basis(const ThpsSkateState *s,int16_t basis[9]) {
 unsigned i;for(i=0;i<3;i++){basis[i*3]=(int16_t)s->source_side[i];basis[i*3+1]=(int16_t)s->source_up[i];basis[i*3+2]=(int16_t)s->source_forward[i];}
}
static void unpack_basis(ThpsSkateState *s,const int16_t basis[9]) {
 unsigned i;for(i=0;i<3;i++){s->source_side[i]=basis[i*3];s->source_up[i]=basis[i*3+1];s->source_forward[i]=basis[i*3+2];}yaw_from_basis(s);
}
static int grind_basis(ThpsSkateState *s,int jumping) {
 int16_t basis[9],roll[9];const int16_t *tilt;
 if(!thps1_grind_basis_q12(basis,jumping?s->grind.travel_direction:s->grind.facing))return 0;
 if(!jumping&&s->grind.turn_q12) {
  if(!s->yaw_rotations)return 0;
  thps1_air_spin_rotate_basis_q12(basis,s->yaw_rotations+((uint32_t)s->grind.turn_q12&4095u)*9);
 }
 if(!jumping&&s->grind.tilt_q12) {
  if(!s->pitch_rotations)return 0;
  tilt=s->pitch_rotations+((uint32_t)s->grind.tilt_q12&4095u)*9;
  if(!s->grind.stance){thps1_grind_roll_from_pitch_q12(roll,tilt);tilt=roll;}
  thps1_air_spin_rotate_basis_q12(basis,tilt);
 }
 unpack_basis(s,basis);return 1;
}
static int handle_spin(ThpsSkateState *s,const ThpsSkateInput *input) {
 Thps1AirSpinInput in;Thps1AirSpinOutput out;memset(&in,0,sizeof(in));
 s->spin.state=s->source_state;s->spin.crouched=s->previous_ollie;s->spin.flags=(int32_t)s->stance_flags;
 s->spin.animation=s->animation;s->spin.frame=s->anim_frame_i;s->spin.animation_rate=(int32_t)s->anim_rate;
 in.dt8=256;in.tick=signed_word(s->ticks);in.analog_x=(int32_t)(input->steer*127);in.analog_y=(int32_t)(-input->forward*127);
 in.held40=input->spin_left||input->spin<0||input->spin_continuous_left;in.held60=input->spin_right||input->spin>0||input->spin_continuous_right;
 in.edge51=((input->spin_left||input->spin<0)&&!s->previous_spin_left)||(input->spin_180_left&&!s->previous_180_left);
 in.edge71=((input->spin_right||input->spin>0)&&!s->previous_spin_right)||(input->spin_180_right&&!s->previous_180_right);
 in.global40=in.held40;in.global60=in.held60;in.held80=input->left;in.held90=input->right;
 in.held10=input->flip;in.held20=input->grab;in.helda0=input->up;in.heldb0=input->down;
 if(!thps1_air_spin(&s->spin,&in,&out))return 1;
 if(out.queued_rotation&&!rotate_basis(s,out.queued_rotation,0,0))return 0;
 if(out.animation_changed)anim_run(s,out.animation,out.from,out.to,out.continuation,65536);
 return 1;
}
static void handle_tricks(ThpsSkateState *s,const ThpsSkateInput *input,const ThpsAnimBank *bank) {
 Thps1TrickInput in;ThpsAnim a;memset(&in,0,sizeof(in));get_anim(s,&a);
 in.tick=signed_word(s->ticks);in.stick_x=(int8_t)(input->steer*127);in.stick_y=(int8_t)(-input->forward*127);
 in.up=input->up;in.down=input->down;in.left=input->left;in.right=input->right;in.flip=input->flip;in.grab=input->grab;
 s->tricks.source_state=s->source_state;s->tricks.airborne=s->jump_latched;s->tricks.blocked=s->bail.active;
 s->tricks.special_enabled=s->scoring.special;
 s->tricks.input_lock=s->spin.input_lock;s->tricks.count_lock=s->spin.count_lock;
 s->tricks.queued180=s->spin.queued180;s->tricks.stance_flags=s->stance_flags;
 thps1_air_tricks(&s->tricks,&a,bank,&in);set_anim(s,&a);
 s->spin.input_lock=s->tricks.input_lock;s->spin.count_lock=s->tricks.count_lock;s->spin.queued180=s->tricks.queued180;
 apply_orientation(s,s->tricks.orientation_effects);
 if(s->tricks.started&&((s->tricks.trick_flags&255u)>=32))s->events|=THPS_EVENT_SPECIAL;
 if(s->tricks.started){s->trick_index=(int32_t)(s->tricks.trick_flags&255);s->events|=(s->tricks.trick_flags&0x2000)?THPS_EVENT_FLIP:THPS_EVENT_GRAB;}
 if(s->tricks.ended)s->events|=THPS_EVENT_TRICK_END;
 if(s->tricks.score_base)s->trick_base_points=s->tricks.score_base;
 s->trick_hold_base_points=s->tricks.hold_score_base;
 if(s->tricks.score_index>=0){
  s->trick_index=s->tricks.score_index;s->events|=THPS_EVENT_TRICK_BASE;
  thps1_score_add(&s->scoring,s->tricks.score_index,s->tricks.score_base);
 }
 if(s->tricks.hold_score_base)thps1_score_hold(&s->scoring,thps1_score_hold_value(&s->scoring,
  (int32_t)(s->tricks.trick_flags&255u),s->tricks.hold_score_base,20));
}

int thps_skate_reset(ThpsSkateState *s,float x,float y,float z,uint16_t yaw,float scale) {
 if(!s||!valid_float(x)||!valid_float(y)||!valid_float(z)||!valid_float(scale)||scale<0.0001f||scale>64)return 0;
 memset(s,0,sizeof(*s));s->position[0]=x;s->position[1]=y;s->position[2]=z;
 s->floor_normal[1]=1;s->yaw=yaw;s->source_to_host=scale;s->valid=1;
 s->stat_ollie=3;s->stat_speed=7;s->stat_air=7;s->stat_balance=4;
 s->kick_target=thps1_ground_speed_from_total(s->stat_speed+17);
 s->anim_rate=65536;s->anim_continuation=-1;s->trick_index=-1;
 thps1_score_init(&s->scoring);basis_from_host(s);pack_basis(s,s->source_physics_basis);thps1_air_tricks_clear(&s->tricks);thps1_grind_reset(&s->grind,1);
 if(!body_from_foot(s,s->source_body_position)){memset(s,0,sizeof(*s));return 0;}
 memcpy(s->source_previous_position,s->source_body_position,sizeof(s->source_body_position));
 memcpy(s->source_earlier_position,s->source_body_position,sizeof(s->source_body_position));return 1;
}
int thps_skate_bind_clip_counts(ThpsSkateState *s,const uint16_t *counts,uint32_t n) {
 unsigned i;if(!s||!s->valid||!counts||n!=78||s->step_pending)return 0;
 for(i=0;i<78;i++)if(!counts[i]||counts[i]>127)return 0;
 if(counts[8]<=26)return 0;
 memcpy(s->clip_counts,counts,sizeof(s->clip_counts));s->counts_bound=1;
 anim_run(s,0,0,-1,-1,65536);return 1;
}
int thps_skate_bind_body_offset(ThpsSkateState *s,float units) {
 ThpsSkateState copy;if(!s||!s->valid||s->step_pending||!isfinite(units)||units<0||units>1000)return 0;
 copy=*s;
 if(!s->body_offset_bound&&!s->ticks&&s->mode==THPS_SKATE_GROUND) {
  unsigned i;float length=0;
  for(i=0;i<3;i++){if(!isfinite(copy.floor_normal[i])||fabsf(copy.floor_normal[i])>1.001f)return 0;length+=copy.floor_normal[i]*copy.floor_normal[i];}
  length=sqrtf(length);if(length<0.5f||copy.floor_normal[1]<=0)return 0;
  for(i=0;i<3;i++)copy.floor_normal[i]/=length;
  basis_from_host(&copy);pack_basis(&copy,copy.source_physics_basis);
 }
 copy.body_offset_source=units;copy.body_offset_bound=1;
 if(!body_from_foot(&copy,copy.source_body_position))return 0;
 copy.source_body_valid=1;memcpy(copy.source_previous_position,copy.source_body_position,sizeof(copy.source_body_position));
 memcpy(copy.source_earlier_position,copy.source_body_position,sizeof(copy.source_body_position));*s=copy;return 1;
}
int thps_skate_bind_rotation_tables(ThpsSkateState *s,const int16_t *pitch,const int16_t *yaw,uint32_t count) {
 uint32_t i;if(!s||!s->valid||s->step_pending||!pitch||!yaw||count!=4096)return 0;
 for(i=0;i<count*9;i++)if(pitch[i]<-4096||pitch[i]>4096||yaw[i]<-4096||yaw[i]>4096)return 0;
 s->pitch_rotations=pitch;s->yaw_rotations=yaw;return 1;
}
void thps_skate_cancel_inputs(ThpsSkateState *s) {
 if(!s)return;
 s->previous_ollie=s->previous_flip=s->previous_grab=s->previous_grind=s->previous_spin_left=s->previous_spin_right=0;s->previous_spin=0;s->previous_180_left=s->previous_180_right=0;
 s->charge_ticks=0;s->grind.charge_ticks=0;s->grind.previous_ollie=0;s->input_cancelled=1;s->events=0;
 clear_trick_queue(&s->tricks);s->spin.queued180=0;s->spin.yaw_rate=s->spin.lean_rate=s->spin.pitch_delta=0;
 memset(&s->step_input,0,sizeof(s->step_input));
 if(s->counts_bound&&s->animation==8)anim_run(s,0,0,-1,-1,65536);
}
static int state_valid(const ThpsSkateState *s) {
 unsigned i;int32_t body[3];
 if(!s||!s->valid||!s->counts_bound||s->animation>=78||s->mode==THPS_SKATE_BAIL||s->mode>THPS_SKATE_GRIND)return 0;
 if(s->scoring.count<0||s->scoring.count>=THPS1_SCORE_ENTRIES||s->scoring.special<0||s->scoring.special>750)return 0;
 if(s->stat_air>13||s->stat_ollie>13||s->stat_speed>13||s->stat_balance>13)return 0;
 if(!isfinite(s->source_to_host)||s->source_to_host<0.0001f||s->source_to_host>64)return 0;
 if(s->body_offset_bound&&(!isfinite(s->body_offset_source)||s->body_offset_source<0||s->body_offset_source>1000))return 0;
 if(s->anim_frame_i<0||s->anim_frame_i>=s->clip_counts[s->animation]||s->anim_rate>1048576)return 0;
 if(s->tricks.read_index<0||s->tricks.read_index>9||s->tricks.write_index<0||s->tricks.write_index>9||(s->tricks.trick_flags&255u)>35)return 0;
 if(s->bail.active&&(s->bail.phase<1||s->bail.phase>9))return 0;
 if(s->charge_ticks<0||s->charge_ticks>15||s->source_turn_rate<-655360||s->source_turn_rate>655360)return 0;
 if(s->spin.queued180<-1000000||s->spin.queued180>1000000||s->spin.yaw_rate<-655360||s->spin.yaw_rate>655360||s->spin.lean_rate<-655360||s->spin.lean_rate>655360)return 0;
 for(i=0;i<78;i++)if(!s->clip_counts[i]||s->clip_counts[i]>127)return 0;
 for(i=0;i<3;i++) {
  if(!isfinite(s->position[i])||!isfinite(s->floor_normal[i])||fabsf(s->floor_normal[i])>1.001f||s->source_velocity[i]>1000000||s->source_velocity[i]<-1000000)return 0;
  if(s->source_forward[i]<-4096||s->source_forward[i]>4096||s->source_side[i]<-4096||s->source_side[i]>4096||s->source_up[i]<-4096||s->source_up[i]>4096)return 0;
  if(s->source_acceleration[i]<-1000000||s->source_acceleration[i]>1000000)return 0;
  if(s->source_body_position[i]<-1000000000||s->source_body_position[i]>1000000000)return 0;
 }
 return body_from_foot(s,body);
}
static int input_valid(const ThpsSkateInput *in) {
 return in&&isfinite(in->steer)&&isfinite(in->forward)&&fabsf(in->steer)<=1&&fabsf(in->forward)<=1&&in->spin>=-1&&in->spin<=1&&
  in->ollie<=1&&in->flip<=1&&in->grab<=1&&in->grind<=1&&in->up<=1&&in->down<=1&&in->left<=1&&in->right<=1&&in->spin_left<=1&&in->spin_right<=1&&in->spin_continuous_left<=1&&in->spin_continuous_right<=1&&in->spin_180_left<=1&&in->spin_180_right<=1;
}
int thps_skate_begin_step(const ThpsSkateState *committed,const ThpsSkateInput *in,ThpsSkateState *pending) {
 return thps_skate_begin_step_with_host(committed,in,0,pending);
}
int thps_skate_begin_step_with_host(const ThpsSkateState *committed,const ThpsSkateInput *in,const ThpsSkateHost *host,ThpsSkateState *pending) {
 ThpsSkateState s;ThpsSkateInput input;Thps1GroundKick kick;Thps1GrindInput gi;
 ThpsAnimBank bank;uint8_t counts[78];int32_t f[3],normal[3],acc[3]={0,0,0};
 int32_t speed,limit,delta[3]={0,0,0},start_body[3];unsigned i;int held,brake,grind_moved=0;
 if(!pending||!input_valid(in)||!state_valid(committed)||committed->step_pending)return 0;
 s=*committed;input=*in;s.events=0;s.step_pending=1;s.step_air=s.step_ground_forces=s.step_skip_host_collision=s.step_new_bail=0;++s.ticks;
 s.trick_base_points=s.trick_hold_base_points=0;memset(s.source_acceleration,0,sizeof(s.source_acceleration));
 s.source_state=s.mode==THPS_SKATE_GROUND?0:s.mode==THPS_SKATE_GRIND?4:1;
 make_bank(&s,counts,&bank);anim_advance(&s);thps1_score_tick(&s.scoring);thps1_score_display_tick(&s.scoring);
 if(s.input_cancelled) {
  int all_released=!input.ollie&&!input.flip&&!input.grab&&!input.grind&&!input.spin&&!input.spin_left&&!input.spin_right&&!input.spin_continuous_left&&!input.spin_continuous_right&&!input.spin_180_left&&!input.spin_180_right;
  memset(&input,0,sizeof(input));if(all_released)s.input_cancelled=0;
 }
 s.step_input=input;held=input.ollie!=0;brake=input.forward<0||input.down;
 if(!s.source_body_valid&&!body_from_foot(&s,s.source_body_position))return 0;
 gi=grind_input(&input);
 if(!s.bail.active) {
  if(s.source_state) {
   s.spin.yaw_rate=s.source_turn_rate;
   if(!handle_spin(&s,&input))return 0;
   s.source_turn_rate=s.spin.yaw_rate;
  } else {
   int down=input.down||(int32_t)(-input.forward*127)>=31;
   int32_t step=thps1_ground_steer_step(256,0,down);
   s.source_turn_rate=thps1_ground_steer_rate(s.source_turn_rate,step,down?368640:184320,(int32_t)(input.steer*127),input.left||input.steer<=-0.999f,input.right||input.steer>=0.999f,down);
   s.spin.yaw_rate=s.spin.lean_rate=s.source_turn_rate;s.spin.pitch_delta=0;
   if(!s.yaw_rotations)basis_from_host(&s);
  }
 }
 speed=source_speed(s.source_velocity);s.source_speed=speed;
 for(i=0;i<3;i++){f[i]=-s.source_forward[i];normal[i]=s.source_up[i];}
 if(s.mode==THPS_SKATE_GROUND&&!s.bail.active) {
  memset(&kick,0,sizeof(kick));kick.timer=s.kick_ticks;kick.target_speed=s.kick_target;
  kick.animation=s.animation;kick.frame=s.anim_frame_i;kick.finished=s.anim_finished;
  kick.animation_rate=(int32_t)s.anim_rate;kick.sound_pending=s.kick_sound_pending;
  kick.speed=speed;kick.slope_dot=0;kick.auto_kick=1;kick.push_button=input.flip;kick.crouched=s.previous_ollie;
  kick.down_and_turning=brake;kick.effective_speed_stat=effective_stat(&s,s.stat_speed);
  thps1_ground_kick(&kick);s.kick_ticks=kick.timer;s.kick_target=kick.target_speed;s.kick_sound_pending=kick.sound_pending;
  if(kick.animation_changed)anim_run(&s,kick.animation,0,-1,-1,kick.animation_rate);
  for(i=0;i<3;i++)acc[i]=f[i]*kick.acceleration_scale;
  {ThpsAnim a;get_anim(&s,&a);if(thps1_landing_finish_animation(&a,&bank,s.previous_ollie)){finish_orientation(&s);set_anim(&s,&a);}}
  if(s.landing_pending) {
   ThpsAnim a;get_anim(&s,&a);finish_orientation(&s);thps1_landing_start_animation(&a,&bank,s.previous_ollie);set_anim(&s,&a);s.landing_pending=0;s.jump_latched=0;
   if(thps1_landing_trick_bails(s.tricks.active,s.tricks.interruptible,s.tricks.trick_flags,input.flip,input.grab,s.tricks.rotation_active,s.tricks.rotation_lock,s.tricks.rotation_direction)) {
    memcpy(s.source_acceleration,acc,sizeof(acc));start_bail(&s,&bank,0,0,0);
   } else {
    s.score_boost_ticks=signed_word((uint32_t)s.score_boost_ticks+(uint32_t)thps1_score_landing_boost(&s.scoring));
    (void)thps1_score_bank(&s.scoring,s.spin.completed180,s.spin.queued_angle,s.landing_previous_state,s.tricks.scored,s.spin.count_lock);
    /* Display tick already decremented any previous delay this frame.
     * Only a bank in this call resets it to40, including a wrapped-zero award. */
    if(s.scoring.bank_delay==40)s.events|=THPS_EVENT_SCORE_BANK;
    s.tricks.active=0;s.tricks.scored=0;s.tricks.trick_flags=0;s.tricks.rotation_active=0;
    s.trick_sequence_ready=0;s.spin.queued_angle=s.spin.completed180=s.spin.input_lock=s.spin.count_lock=0;
   }
  }
  if(!s.bail.active&&held) {
   limit=thps1_ollie_charge_limit(effective_stat(&s,s.stat_air),effective_stat(&s,s.stat_ollie));if(s.charge_ticks<limit)++s.charge_ticks;
   if(!s.previous_ollie)anim_run(&s,8,0,26,19,65536);
   s.kick_target=thps1_ground_speed_from_total(effective_stat(&s,s.stat_speed)+22);
  } else if(!s.bail.active&&s.previous_ollie&&s.charge_ticks>0) {
   s.source_velocity[1]-=thps1_ollie_impulse(effective_stat(&s,s.stat_air),effective_stat(&s,s.stat_ollie),s.charge_ticks,abs(normal[1])<2500);
   s.mode=THPS_SKATE_AIR;s.source_state=1;s.air_ticks=0;s.events|=THPS_EVENT_OLLIE;s.charge_ticks=0;s.jump_latched=1;s.trick_sequence_ready=1;s.spin.input_lock=s.spin.count_lock=0;
   begin_score(&s);anim_run(&s,4,0,-1,-1,65536);
  }
 }
 /* Original HandleJump executes before HandleTricks, including grind ollie. */
 if(s.mode==THPS_SKATE_GRIND&&!s.bail.active) {
  s.grind.ollie_stat=effective_stat(&s,s.stat_ollie);s.grind.air_stat=effective_stat(&s,s.stat_air);
  int32_t prior_charge=s.grind.charge_ticks;int result;
  memcpy(s.grind.velocity,s.source_velocity,sizeof(s.source_velocity));get_anim(&s,&s.grind.animation);
  result=thps1_grind_handle_jump(&s.grind,&gi,&bank);if(result<0)return 0;
  s.charge_ticks=s.grind.charge_ticks;
  if(result) {
   begin_score(&s);finish_orientation(&s);
   memcpy(s.source_velocity,s.grind.velocity,sizeof(s.source_velocity));set_anim(&s,&s.grind.animation);
   if(!grind_basis(&s,1))return 0;
   if(s.grind.dismount_turn_q12) {
    int32_t impulse=thps1_ollie_impulse(effective_stat(&s,s.stat_air),effective_stat(&s,s.stat_ollie),prior_charge,0);const int16_t *rotation;
    if(!s.yaw_rotations)return 0;
    rotation=s.yaw_rotations+((uint32_t)s.grind.dismount_turn_q12&4095u)*9;
    s.source_velocity[1]+=impulse;thps1_grind_turn_velocity_q12(s.source_velocity,s.source_physics_basis,rotation);s.source_velocity[1]-=impulse;
    if(!rotate_basis(&s,s.grind.dismount_turn_q12,0,0))return 0;
   }
   s.tricks.active=0;s.tricks.end_block=0;grind_air_history(&s.tricks);
   s.events|=THPS_EVENT_OLLIE;s.mode=THPS_SKATE_AIR;s.source_state=1;s.air_ticks=0;s.jump_latched=1;s.trick_sequence_ready=1;s.spin.input_lock=s.spin.count_lock=0;
  }
 }
 if(!s.bail.active)handle_tricks(&s,&input,&bank);
 /* A pre-physics landing recheck bail still runs this tick's air collision. */
 s.step_new_bail=0;
 /* Snapshot original +2a8 <- +16c; +16c <- current +4, before movement. */
 pack_basis(&s,s.source_physics_basis);
 thps1_landing_history_shift(s.source_body_position,s.source_previous_position,s.source_earlier_position);
 memcpy(start_body,s.source_body_position,sizeof(start_body));
 /* Host geometry was queried before main; actual source entry5ad4c is
  * AFTER helpers and history snapshot, immediately before physics dispatch. */
 if(host&&host->grind_candidate&&host->grind_candidate->candidate_valid) {
  Thps1GrindEntry e=*host->grind_candidate;Thps1GrindState proposed;int result;
  int32_t entry_forward[3],entry_side[3];uint32_t entry_stance=s.stance_flags;
  if(!s.body_offset_bound)return 0;
  memcpy(e.actor_position,s.source_body_position,sizeof(e.actor_position));memcpy(e.velocity,s.source_velocity,sizeof(e.velocity));memcpy(entry_forward,s.source_forward,sizeof(entry_forward));memcpy(entry_side,s.source_side,sizeof(entry_side));
  (void)thps1_ground_stance_reorient(s.animation,entry_forward,entry_side,s.source_velocity,&entry_stance);
  memcpy(e.right,entry_side,sizeof(e.right));
  e.ready=1;e.source_state=s.source_state;e.bailed=(uint8_t)(s.bail.active!=0);e.switch_stance=(uint8_t)((entry_stance&2)!=0);e.input_locked=(uint8_t)(s.input_cancelled!=0);
  s.grind.charge_ticks=s.charge_ticks;s.grind.balance_stat=effective_stat(&s,s.stat_balance);s.grind.speed_stat=effective_stat(&s,s.stat_speed);s.grind.ollie_stat=effective_stat(&s,s.stat_ollie);s.grind.air_stat=effective_stat(&s,s.stat_air);get_anim(&s,&s.grind.animation);
  proposed=s.grind;result=thps1_grind_enter(&proposed,&e,&gi,&bank);if(result<0)return 0;
  s.grind.cooldown=proposed.cooldown;
  if(result) {
   const Thps1TrickRecord *record=thps1_trick_record(s.tricks.trick_flags&255u);
   if(thps1_grind_entry_bail(s.tricks.active,s.tricks.interruptible,s.anim_frame_i,s.clip_counts[s.animation],s.tricks.trick_flags,record->landing_frames,s.tricks.rotation_active,s.tricks.rotation_lock,s.tricks.rotation_direction)) {
    memcpy(s.source_acceleration,acc,sizeof(acc));start_bail(&s,&bank,0,0,0);result=0;
   } else if(!s.trick_sequence_ready)result=0;
  }
  if(result) {
   s.grind=proposed;
   memcpy(s.source_forward,entry_forward,sizeof(entry_forward));memcpy(s.source_side,entry_side,sizeof(entry_side));s.stance_flags=entry_stance;
   s.mode=THPS_SKATE_GRIND;s.source_state=4;s.grind_rail=e.rail;s.events|=THPS_EVENT_GRIND;
   memcpy(s.source_body_position,s.grind.position,sizeof(s.source_body_position));memcpy(s.source_velocity,s.grind.velocity,sizeof(s.source_velocity));
   s.source_body_valid=1;set_anim(&s,&s.grind.animation);
   if(s.tricks.active&&!s.tricks.scored&&(s.tricks.trick_flags&0x6000u)) {
    const Thps1TrickRecord *record=thps1_trick_record(s.tricks.trick_flags&255u);
    s.trick_index=(int32_t)(s.tricks.trick_flags&255u);s.trick_base_points=record->base_points;s.events|=THPS_EVENT_TRICK_BASE;
    thps1_score_add(&s.scoring,s.trick_index,s.trick_base_points);
    s.tricks.trick_count=signed_word((uint32_t)s.tricks.trick_count+1u);
   }
   thps1_score_add(&s.scoring,s.grind.trick+0x42,grind_points(s.grind.trick));
   memcpy(s.grind_score_position,s.grind.position,sizeof s.grind_score_position);
   finish_orientation(&s);
   s.tricks.active=1;s.tricks.scored=1;s.tricks.trick_flags=0x80001000u;
  }
 } else if(input.grind&&!s.bail.active&&!s.input_cancelled&&s.grind.cooldown>0)--s.grind.cooldown;
 /* Common source9e8 age increments before dispatch. Ground immediately
  * resets all balance fields; rail motion owns its own single increment. */
 if(s.mode==THPS_SKATE_GROUND)thps1_grind_ground_reset(&s.grind);
 else if(s.mode!=THPS_SKATE_GRIND)s.grind.balance_ticks=signed_word((uint32_t)s.grind.balance_ticks+1u);
 /* Any entry-gate bail precedes this tick's air contact dispatch. */
 s.step_new_bail=0;
 thps1_score_boost_step(&s.score_boost_ticks,s.source_velocity,acc);
 if(s.mode==THPS_SKATE_GRIND) {
  int32_t distance[3];
  for(i=0;i<3;i++)distance[i]=s.source_body_position[i]-s.grind_score_position[i];
  if(thps1_score_grind_distance(distance)>=31) {
   thps1_score_hold(&s.scoring,thps1_score_hold_value(&s.scoring,s.grind.trick+0x42,grind_points(s.grind.trick),5));
   memcpy(s.grind_score_position,s.source_body_position,sizeof s.grind_score_position);
  }
  s.grind.balance_stat=effective_stat(&s,s.stat_balance);s.grind.speed_stat=effective_stat(&s,s.stat_speed);
  s.grind.ollie_stat=effective_stat(&s,s.stat_ollie);s.grind.air_stat=effective_stat(&s,s.stat_air);
  memcpy(s.grind.velocity,s.source_velocity,sizeof(s.source_velocity));get_anim(&s,&s.grind.animation);
  if(!thps1_grind_motion_pre_final(&s.grind,&s.grind_rail,&gi,&bank,signed_word(s.ticks),host?host->rail_lookup:0,host?host->rail_context:0))return 0;
  if(s.grind.rail_id!=s.grind_rail.id) {
   if(!host||!host->rail_lookup||!host->rail_lookup(host->rail_context,s.grind.rail_id,&s.grind_rail))return 0;
  }
  memcpy(s.source_body_position,s.grind.position,sizeof(s.source_body_position));memcpy(s.source_velocity,s.grind.velocity,sizeof(s.source_velocity));memcpy(acc,s.grind.acceleration,sizeof(acc));set_anim(&s,&s.grind.animation);
  if(!grind_basis(&s,0))return 0;
  if(s.grind.events&THPS1_GRIND_BAIL) {
   memcpy(s.source_acceleration,acc,sizeof(acc));start_bail(&s,&bank,0,0,0);
  } else {
   s.step_skip_host_collision=1;
   if(!s.grind.active){s.mode=THPS_SKATE_AIR;s.source_state=1;s.tricks.active=0;grind_air_history(&s.tricks);s.events|=THPS_EVENT_GRIND_END;}
  }
  grind_moved=1;for(i=0;i<3;i++)delta[i]=s.source_body_position[i]-start_body[i];
 }
 if(s.mode==THPS_SKATE_GROUND) {
  if(!s.bail.active) {
   if(brake&&input.steer==0&&!input.left&&!input.right) {
    int32_t threshold=thps1_ground_brake_threshold(normal[1]);
    if(speed<=threshold&&threshold==40960){memset(s.source_velocity,0,sizeof(s.source_velocity));memset(acc,0,sizeof(acc));}
    else if(speed>threshold)for(i=0;i<3;i++)s.source_velocity[i]=thps1_ground_brake_component(s.source_velocity[i],0,256);
   }
  }
  for(i=0;i<3;i++)delta[i]=thps1_ground_integrate_position(0,s.source_velocity[i],acc[i],256,256);
  if(!s.bail.active) {
   if(thps1_ground_stance_reorient(s.animation,s.source_forward,s.source_side,s.source_velocity,&s.stance_flags))yaw_from_basis(&s);
   if(s.yaw_rotations) {if(!rotate_basis(&s,s.source_turn_rate>>12,0,0))return 0;}
   else {s.yaw=(uint16_t)(s.yaw+(s.source_turn_rate>>12)*16);basis_from_host(&s);}
   s.step_ground_forces=1;
  }
 } else if(s.mode==THPS_SKATE_AIR&&!s.step_skip_host_collision&&!grind_moved) {
  int32_t body[3];s.air_ticks=signed_word((uint32_t)s.air_ticks+1u);
  for(i=0;i<3;i++){delta[i]=thps1_ground_integrate_position(0,s.source_velocity[i],acc[i],256,256);body[i]=start_body[i]+delta[i];}
  if(!s.bail.active) {
   if(!rotate_basis(&s,s.spin.pitch_delta,1,body))return 0;
   if(!rotate_basis(&s,s.spin.yaw_rate>>12,0,0))return 0;
   (void)thps1_air_spin_accumulate_yaw(&s.spin);
  }
  for(i=0;i<3;i++)delta[i]=body[i]-start_body[i];
  s.step_air=1;
 }
 memcpy(s.source_acceleration,acc,sizeof(acc));
 for(i=0;i<3;i++) {
  float sign=i==1?-1.0f:1.0f;
  s.delta[i]=(float)delta[i]*(s.source_to_host/4096)*sign;
  s.velocity[i]=(float)s.source_velocity[i]*(s.source_to_host/4096)*sign;
  s.source_body_position[i]=committed->source_body_position[i]+delta[i];
 }
 sync_score(&s);
 s.source_body_valid=s.body_offset_bound;s.spin_total=signed_word((uint32_t)s.spin.completed180*180u);
 s.previous_ollie=(uint8_t)(!s.bail.active&&held);s.previous_flip=input.flip;s.previous_grab=input.grab;s.previous_grind=input.grind;
 s.previous_spin=input.spin;s.previous_spin_left=(uint8_t)(input.spin_left||input.spin<0);s.previous_spin_right=(uint8_t)(input.spin_right||input.spin>0);s.previous_180_left=input.spin_180_left;s.previous_180_right=input.spin_180_right;
 if(s.animation>=78||s.anim_frame_i<0||s.anim_frame_i>=s.clip_counts[s.animation])return 0;
 *pending=s;return 1;
}
int thps_skate_resolve(ThpsSkateState *pending,const ThpsSkateContact *c) {
 ThpsSkateState s;ThpsAnimBank bank;uint8_t counts[78];unsigned i;float len;int contact_ground;
 if(!state_valid(pending)||!pending->step_pending||!c||!c->valid)return 0;
 for(i=0;i<3;i++)if(!isfinite(c->position[i])||!isfinite(c->floor_normal[i])||!isfinite(c->wall_normal[i])||fabsf(c->floor_normal[i])>1.001f||fabsf(c->wall_normal[i])>1.001f)return 0;
 s=*pending;make_bank(&s,counts,&bank);
 contact_ground=c->grounded&&!s.step_skip_host_collision;
 if(s.step_skip_host_collision)foot_from_body(&s);else memcpy(s.position,c->position,sizeof(s.position));
 if(c->hit_ceiling&&s.source_velocity[1]<0)s.source_velocity[1]=0;
 if(c->hit_wall) {
  /* Explicit host wall blocking, not reconstructed THPS wallride response. */
  float into=s.velocity[0]*c->wall_normal[0]+s.velocity[2]*c->wall_normal[2];s.events|=THPS_EVENT_WALL;
  if(into<0)for(i=0;i<3;i+=2) {
   double removal=(double)into*c->wall_normal[i]*4096/s.source_to_host;
   if(removal < -1000000||removal>1000000)return 0;
   s.source_velocity[i]-=(int32_t)removal;
  }
 }
 if(contact_ground&&(s.mode==THPS_SKATE_GROUND||s.source_velocity[1]>=0)) {
  int32_t normal[3];int was_air=s.mode==THPS_SKATE_AIR;
  len=sqrtf(c->floor_normal[0]*c->floor_normal[0]+c->floor_normal[1]*c->floor_normal[1]+c->floor_normal[2]*c->floor_normal[2]);
  if(len<0.5f||c->floor_normal[1]<=0)return 0;
  for(i=0;i<3;i++){s.floor_normal[i]=c->floor_normal[i]/len;normal[i]=(int32_t)(s.floor_normal[i]*4096)*(i==1?-1:1);}
  if(was_air&&!s.bail.active) {
   Thps1LandingInput in;Thps1LandingResult result;memset(&in,0,sizeof(in));
   memcpy(in.velocity,s.source_velocity,sizeof(in.velocity));memcpy(in.normal,normal,sizeof(in.normal));
   memcpy(in.forward,s.source_forward,sizeof(in.forward));memcpy(in.side,s.source_side,sizeof(in.side));memcpy(in.up,s.source_up,sizeof(in.up));
   in.source_state=s.source_state;in.stance_flags=(int32_t)s.stance_flags;in.trick_active=s.tricks.active;in.trick_interruptible=s.tricks.interruptible;
   in.trick_flags=s.tricks.trick_flags;in.flip_held=s.step_input.flip;in.grab_held=s.step_input.grab;
   in.rotation_active=s.tricks.rotation_active;in.rotation_angle=s.tricks.rotation_lock;in.rotation_direction=s.tricks.rotation_direction;
   thps1_landing_check(&in,&result);s.landing_reason=result.reason;
   if(result.reason!=THPS1_LANDING_CLEAN) {
    start_bail(&s,&bank,0,result.bail_phase_override,result.bail_clip_override);
    if(result.reason==THPS1_LANDING_ALIGNMENT)thps1_landing_alignment_basis(s.source_previous_position,s.source_earlier_position,normal,s.source_forward,s.source_side,s.source_up);
    if(result.reason==THPS1_LANDING_UPSIDE_DOWN) {
     thps1_landing_upside_basis(s.source_forward,s.source_up,s.source_forward,s.source_up);
     memcpy(s.source_velocity,result.override_velocity,sizeof(s.source_velocity));
    }
    yaw_from_basis(&s);
   } else {
    s.landing_previous_state=s.source_state;
    s.events|=THPS_EVENT_LAND;s.charge_ticks=0;s.mode=THPS_SKATE_GROUND;s.source_state=0;
    memcpy(s.source_velocity,result.tangent_velocity,sizeof(s.source_velocity));
    s.landing_pending=(uint8_t)thps1_landing_cleanup_ready(s.jump_latched,1,0);
    thps1_landing_clean_basis(s.source_forward,normal,s.source_forward,s.source_side,s.source_up);yaw_from_basis(&s);
   }
  } else if(!s.step_new_bail) {
   s.mode=THPS_SKATE_GROUND;s.source_state=0;
   if(!s.bail.active)thps1_landing_project(s.source_velocity,normal,s.source_velocity);
  }
 } else if(!contact_ground&&s.mode==THPS_SKATE_GROUND) {
  s.mode=THPS_SKATE_AIR;s.source_state=1;s.air_ticks=0;s.jump_latched=1;s.trick_sequence_ready=1;s.spin.input_lock=s.spin.count_lock=0;
  s.spin.queued_angle=s.spin.completed180=0;s.tricks.active=0;
  if(!s.bail.active){begin_score(&s);anim_run(&s,s.previous_ollie?26:27,0,-1,-1,65536);}
 }
 if(s.step_ground_forces&&!s.bail.active) {
  int32_t f[3],n[3],gravity[3]={0,6500,0};int32_t projected,gn,drag;
  if(s.yaw_rotations) {
   for(i=0;i<3;i++)n[i]=(int32_t)(s.floor_normal[i]*4096)*(i==1?-1:1);
   thps1_landing_clean_basis(s.source_forward,n,s.source_forward,s.source_side,s.source_up);
   for(i=0;i<3;i++){f[i]=-s.source_forward[i];n[i]=s.source_up[i];}yaw_from_basis(&s);
  } else host_basis(&s,f,n);
  projected=fixed_dot(s.source_velocity,f);
  for(i=0;i<3;i++)s.source_velocity[i]=fixed_mul(f[i],projected);
  gn=fixed_dot(gravity,n);drag=s.source_speed>thps1_ground_speed_from_total(effective_stat(&s,s.stat_speed)+39)?82:8;
  for(i=0;i<3;i++)s.source_acceleration[i]+=gravity[i]-fixed_mul(n[i],gn)-fixed_mul(s.source_velocity[i],drag);
 }
 if(s.step_air&&!contact_ground&&!c->hit_wall&&!c->hit_ceiling)s.source_acceleration[1]+=13000;
 if(s.bail.active)run_bail(&s,&bank);
 for(i=0;i<3;i++)s.source_velocity[i]=thps1_ground_integrate_velocity(s.source_velocity[i],s.source_acceleration[i],256);
 {int32_t speed=source_speed(s.source_velocity),cap=thps1_ground_speed_from_total(50+effective_stat(&s,s.stat_speed));
  if(speed>cap)for(i=0;i<3;i++)s.source_velocity[i]=thps1_ground_cap_component(s.source_velocity[i],cap,speed);
 }
 if(contact_ground&&s.mode==THPS_SKATE_GROUND&&!s.bail.active) {
  /* Retain the first-slice host plane boundary only for clean ground physics. */
  int32_t n[3]={(int32_t)(s.floor_normal[0]*4096),(int32_t)(-s.floor_normal[1]*4096),(int32_t)(s.floor_normal[2]*4096)};
  thps1_landing_project(s.source_velocity,n,s.source_velocity);
 }
 for(i=0;i<3;i++)s.velocity[i]=(float)s.source_velocity[i]*(s.source_to_host/4096)*(i==1?-1.0f:1.0f);
 if(!s.step_skip_host_collision&&!body_from_foot(&s,s.source_body_position))return 0;
 sync_score(&s);
 s.source_body_valid=s.body_offset_bound;s.source_speed=source_speed(s.source_velocity);s.step_pending=0;
 if(!state_valid(&s))return 0;
 *pending=s;return 1;
}
const char *thps_skate_mode_name(uint8_t mode) {
 switch(mode){case THPS_SKATE_GROUND:return "rolling";case THPS_SKATE_AIR:return "airborne";case THPS_SKATE_BAIL:return "bail";case THPS_SKATE_GRIND:return "grinding";default:return "invalid";}
}
