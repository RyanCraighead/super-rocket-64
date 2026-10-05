/* Fresh C expression of THPS1 USA Rev1 Tony-profile HandleTricks control flow.
 * See air_tricks_evidence.md and specials_evidence.md for source evidence. */
#include "air_tricks.h"
#include <string.h>
#define EMPTY {78,-1,0,0,0,0,""}
/* Main0x80059bec installs Tony profile type0 into entries6,9,10 before
 * HandleTricks: original records0x800d62f0,+2c,+58 respectively. */
static const Thps1TrickRecord records[36]={
 EMPTY,{53,65536,0,7,10,250,"360 SHOVE IT"},
 {22,65536,0,3,6,250,"IMPOSSIBLE"},EMPTY,
 {20,65536,0,6,10,100,"KICKFLIP"},
 {28,65536,28,8,0,500,"KICKFLIP TO INDY"},
 {60,65536,0,9,9,800,"VARIAL"},EMPTY,
 {21,65536,0,6,10,100,"HEELFLIP"},
 {61,65536,26,8,8,500,"FINGER FLIP"},
 {59,65536,0,8,8,600,"FRONT FOOT IMPOSSIBLE"},EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,
 EMPTY,{31,65536,0,4,0,350,"JAPAN AIR"},
 {32,65536,0,5,0,300,"TAILGRAB"},EMPTY,
 {18,98304,0,5,0,300,"METHOD"},
 {30,65536,0,4,0,500,"MADONNA"},
 {29,65536,0,4,0,300,"STALEFISH"},EMPTY,
 {19,98304,0,6,0,300,"INDY NOSEBONE"},
 {24,65536,0,4,0,350,"ROCKET AIR"},
 {23,65536,14,4,0,400,"BENIHANA"},EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,
 /* Original special records32..35. Braces are original display markup. */
 {65,65536,0,5,0,8000,"{THE 900}"},
 {62,65536,0,5,0,4000,"{KICKFLIP MCTWIST}"},
 {63,65536,0,5,0,2000,"{540 BOARD VARIAL}"},
 {64,65536,0,5,0,1500,"{360 FLIP TO MUTE}"}
};
#undef EMPTY
static int32_t s32(uint32_t x){return x<=INT32_MAX?(int32_t)x:(int32_t)((int64_t)x-4294967296LL);}
static int32_t difference(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
const Thps1TrickRecord *thps1_trick_record(unsigned i){return i<36?&records[i]:0;}
void thps1_air_tricks_begin_sequence(Thps1AirTricks *s,int32_t tick){
 s->queued180=s->read_index=s->write_index=s->buttons[0]=s->directions[0]=0;
 if(difference(tick,s->history[6])>=9)s->history[9]=s->history[8]=s->history[6]=0;
 s->history[2]=s->history[1]=s->history[5]=s->history[4]=0;
}
uint32_t thps1_air_tricks_finish_orientation(Thps1AirTricks *s){
 uint32_t effects=0;
 if(s->pending_stance_toggle){s->stance_flags^=2u;s->pending_stance_toggle=0;effects|=1u;}
 if(s->pending_basis_flip){s->pending_basis_flip=0;effects|=2u;}
 s->orientation_effects^=effects;return effects;
}
void thps1_air_tricks_apply_orientation(int16_t basis[9],uint32_t effects){
 unsigned row,col;if(!(effects&2u))return;
 for(row=0;row<3;row++)for(col=0;col<3;col+=2){
  uint32_t low=(0u-(uint32_t)(int32_t)basis[row*3+col])&65535u;
  basis[row*3+col]=(int16_t)(low<32768u?(int32_t)low:(int32_t)low-65536);
 }
}
void thps1_air_tricks_clear(Thps1AirTricks *s){memset(s,0,sizeof(*s));s->score_index=-1;}
void thps1_air_tricks(Thps1AirTricks *s,ThpsAnim *a,const ThpsAnimBank *bank,const Thps1TrickInput *in){
 int32_t dir=(int32_t)in->up+2*(int32_t)in->down+4*(int32_t)in->left+8*(int32_t)in->right;
 int32_t button;int w=s->write_index,r,index;uint32_t phase;const Thps1TrickRecord *rec;
 s->started=s->ended=s->score_base=s->hold_score_base=0;s->score_index=-1;s->orientation_effects=0;
 if(!s->active&&!s->airborne)s->trick_count=0;
 if(s->delay){s->delay=s32((uint32_t)s->delay-1u);return;}
 if(s->rotation_lock)return;
 if(in->stick_y < -40)dir|=1;
 if(in->stick_y > 40)dir|=2;
 if(in->stick_x < -40)dir|=4;
 if(in->stick_x > 40)dir|=8;
 if(dir){
  if(s->history[9]){
   if(s->history[8]){
    s->history[0]=s->history[3];s->history[1]=s->history[4];s->history[2]=s->history[5];
    s->history[3]=s->history[7];s->history[4]=s->history[6];s->history[5]=s->history[8];
    s->history[6]=s->history[8]=0;
   }
   if(!s->history[6]){s->history[7]=dir;s->history[6]=in->tick;s->history[8]=0;}
  }
  s->directions[w]=dir;s->direction_ticks[w]=in->tick;
  if(s->buttons[w]&&difference(in->tick,s->button_ticks[w])>=21)s->buttons[w]=0;
 }else{
  if(s->history[6]&&!s->history[8])s->history[8]=in->tick;
  else s->history[9]=1;
 }
 if(s->blocked||!s->airborne||s->source_state==4||s->source_state==5||s->source_state==8){s->release_debounce=0;return;}
 button=(int32_t)in->grab+2*(int32_t)in->flip;
 if(button){
  s->buttons[w]=button;s->button_ticks[w]=in->tick;
  if(s->directions[w]&&difference(in->tick,s->direction_ticks[w])>=21)s->directions[w]=0;
 }else if(s->release_debounce>0){s->directions[w]=s->buttons[w]=0;--s->release_debounce;}
 if(s->directions[w]&&s->buttons[w]&&!s->release_debounce){
  if(s->history[5]&&s->history[5]<s->direction_ticks[w]&&
     difference(s->history[5],s->history[4])<20&&difference(s->direction_ticks[w],s->history[5])<30)
   s->directions[w]|=s32((uint32_t)s->history[3]<<4);
  s->history[1]=s->history[2]=s->history[4]=s->history[5]=s->history[6]=s->history[8]=s->history[9]=0;
  if(++w==10)w=0;
  s->write_index=w;s->buttons[w]=s->directions[w]=0;s->release_debounce=3;
 }
 r=s->read_index;
 if(r!=s->write_index&&(!s->active||s->interruptible)){
  int special=-1;int grab;uint32_t flags=0;
  index=s->directions[r]&15;button=s->buttons[r];
  if(s->special_enabled&&(s->directions[r]&~15)){
   /* Source scans32..65, key = profile0 + prior*256 + current*16 + button.
    * Exact match only; simultaneous flip/grab(3) is not either air trigger. */
   uint32_t key=((uint32_t)s->directions[r]<<4)+(uint32_t)button;
   if(key==0x821u)special=32;
   else if(key==0x881u)special=33;
   else if(key==0x442u)special=34;
   else if(key==0x282u)special=35;
  }
  if(special>=0){index=special;grab=button!=2;flags=index<34?0xf00000u:0;}
  else{
   /* A missed special falls through after the table's -1 sentinel. The
    * original grab branch tests the unoffset flip record BEFORE adding16. */
   if((button&1)&&records[index].clip!=78)index+=16;
   else if(!(button&2)||records[index].clip==78)index=-1;
   grab=index>=16;
  }
  if(index>=0){
   rec=&records[index];a->rate=(uint32_t)rec->rate;
   s->trick_flags=(uint32_t)index|(grab?0x4000u:0x2000u);
   thps1_air_tricks_finish_orientation(s);
   thps1_anim_run(a,bank,rec->clip,0,rec->hold_frame?rec->hold_frame:-1,-1);
   s->trick_flags|=rec->hold_frame?0x40000000u:(grab?0x20000000u:0x80000000u);
   s->input_lock=(int32_t)((flags>>20)&1u);s->count_lock=(int32_t)((flags>>21)&1u);
   s->hold_disabled=(int32_t)((flags>>26)&1u);
   if(s->input_lock)s->queued180=0;
   if(flags&0x800000u)s->trick_flags=(s->trick_flags&0x00ffbfffu)|0x80002000u;
   s->pending_stance_toggle=(int32_t)((flags>>24)&1u);
   s->pending_basis_flip=(int32_t)((flags>>22)&1u);
   if(special>=0){s->rotation_active=0;s->rotation_direction=index>=34?128:0;s->rotation_lock=0;}
   s->started_tick=in->tick;s->active=1;s->scored=s->interruptible=0;
   s->directions[r]=s->buttons[r]=0;if(++r==10)r=0;s->read_index=r;s->started=1;
  }
 }
 if(!s->active)return;
 index=(int)(s->trick_flags&255u);rec=&records[index];phase=s->trick_flags&0xff000000u;
 if((phase==0x20000000u||phase==0x40000000u)&&a->finished)s->interruptible=1;
 if(a->direction==-1)s->interruptible=1;
 else{
  if((s->trick_flags&0x4000u)&&rec->hold_frame&&a->frame>=(int32_t)a->count-rec->interrupt_frames-1)s->interruptible=1;
  if((s->trick_flags&0x2000u)&&a->frame>=(int32_t)a->count-rec->interrupt_frames-1)s->interruptible=1;
 }
 if(s->interruptible){
  /* Original0x8004f030 except downstream score/HUD call0x8004cacc. */
  if(!s->scored&&(s->trick_flags&0x6000u)){
   s->trick_count=s32((uint32_t)s->trick_count+1u);s->score_index=index;s->score_base=rec->base_points;
   if((s->trick_flags&0x4000u)||rec->hold_frame)s->trick_flags|=0x800u;
  }
  s->scored=1;
 }
 /* Original0x8004ef10's initial hold eligibility. Its repetition penalties and
  * score engine are outside this API; emit the unreduced original table base. */
 if(!s->hold_disabled&&(s->trick_flags&0x800u))s->hold_score_base=rec->base_points;
 if(!a->finished)return;
 if(phase==0x40000000u){
  if((s->trick_flags&0x4000u)?in->grab:in->flip)s->trick_flags|=0x800u;
  else s->trick_flags&=~0x800u;
  if(s->trick_flags&0x800u)return;
  if(a->frame<=rec->hold_frame){thps1_air_tricks_finish_orientation(s);thps1_anim_run(a,bank,a->id,rec->hold_frame,-1,-1);return;}
 }else if(phase==0x20000000u){
  if(in->grab)s->trick_flags|=0x800u;else s->trick_flags&=~0x800u;
  if(s->trick_flags&0x800u)return;
  if(!(s->trick_flags&0x400u)){
   thps1_air_tricks_finish_orientation(s);thps1_anim_run(a,bank,a->id,a->frame,0,-1);s->trick_flags|=0x400u;return;
  }
 }else if(phase!=0x80000000u)return;
 if(!s->end_block){a->rate=65536;thps1_air_tricks_finish_orientation(s);thps1_anim_run(a,bank,14,0,-1,-1);s->active=0;s->ended=1;}
}
