#include "score_controller.h"
#include <string.h>
/* Explicit low-word operations avoid signed-overflow UB and implementation-
 * defined conversion for the original MIPS addu/subu/mult behavior. */
static int32_t signed_word(uint32_t x) {
 return x <= INT32_MAX ? (int32_t)x : -1 - (int32_t)(UINT32_MAX-x);
}
static int32_t add(int32_t a,int32_t b) { return signed_word((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return signed_word((uint32_t)a-(uint32_t)b); }
static int32_t mul(int32_t a,int32_t b) { return signed_word((uint32_t)a*(uint32_t)b); }
void thps1_score_init(Thps1Score *s) { memset(s,0,sizeof(*s)); }
void thps1_score_begin(Thps1Score *s) {
 s->enabled=1;s->blocked=0;s->score_mode=0;
 memset(s->attempt_repetitions,0,sizeof(s->attempt_repetitions));
 if(s->game_mode==2)memset(s->repetitions,0,sizeof(s->repetitions));
}
int32_t thps1_score_add(Thps1Score *s,int32_t index,int32_t base) {
 int32_t n,p;
 if(!s->enabled || index<0 || index>=THPS1_SCORE_TRICKS)return 0;
 s->repetitions[index]=add(s->repetitions[index],1);
 s->attempt_repetitions[index]=add(s->attempt_repetitions[index],1);
 n=s->repetitions[index];p=base;
 if(base<0)p=sub(0,base);
 else if(n==2)p=mul(base,75)/100;
 else if(n==3)p=base/2;
 else if(n==4)p=base/4;
 else if((uint32_t)n>=5)p=base/10;
 if(s->blocked==4) {
  p=0;s->repetitions[index]=sub(s->repetitions[index],1);
  s->attempt_repetitions[index]=sub(s->attempt_repetitions[index],1);
 }
 s->hud_active=1;
 if(s->score_mode>=0 && s->score_mode<3 && p>0) {
  if(!s->count) {
   s->settled_total+=s->pending;s->pending=0;s->last_multiplier2=0;
   s->bank_delay=0;s->result=0;
  }
  s->entry_points[s->count]=p;s->entry_index[s->count]=index;
  s->entry_spin_degrees[s->count]=index==74 && base<0 && base>=-350 && base%50==0 ? (-base/50)*180 : 0;
  if(s->count<19)s->count++;
 }
 return p;
}
int32_t thps1_score_hold_value(const Thps1Score *s,int32_t index,int32_t base,int32_t divisor) {
 int32_t n,p=base;
 if(index<0 || index>=THPS1_SCORE_TRICKS || divisor<=0)return 0;
 n=s->repetitions[index];
 if(n==2)p=mul(base,75)/100;
 else if(n==3)p=base/2;
 else if(n==4)p=base/4;
 else if((uint32_t)n>=5)p=base/10;
 return p/divisor;
}
void thps1_score_hold(Thps1Score *s,int32_t delta) {
 if(s->count>0 && s->count<THPS1_SCORE_ENTRIES)
  s->entry_points[s->count-1]=add(s->entry_points[s->count-1],delta);
}
int32_t thps1_score_base(const Thps1Score *s) {
 int32_t sum=0;int i;
 for(i=0;i<s->count && i<THPS1_SCORE_ENTRIES;i++)sum=add(sum,s->entry_points[i]);
 return sum;
}
int32_t thps1_score_multiplier2(int32_t count,int32_t half_turns) {
 return add(mul(count,2),half_turns<3?half_turns:sub(mul(half_turns,2),2));
}
uint32_t thps1_score_preview(const Thps1Score *s,int32_t half_turns) {
 return (uint32_t)mul(thps1_score_base(s),thps1_score_multiplier2(s->count,half_turns))>>1;
}
uint32_t thps1_score_total(const Thps1Score *s) { return s->settled_total+s->pending; }
int32_t thps1_score_landing_boost(const Thps1Score *s) {
 int32_t n=thps1_score_base(s)/100;return n<21?n:20;
}
uint32_t thps1_score_bank(Thps1Score *s,int32_t half_turns,int32_t residual,
                        int32_t previous_state,int32_t scored,int32_t suppress_round) {
 uint32_t award=0;int32_t abs_residual=residual<0?sub(0,residual):residual;
 if(!suppress_round && abs_residual>=1501)half_turns=add(half_turns,1);
 if(half_turns==1 && previous_state==2)half_turns=0;
 if(!scored && half_turns && (previous_state!=2 || half_turns>=2)) {
  int32_t pure=half_turns;half_turns=0;
  if((uint32_t)sub(pure,1)<7)thps1_score_add(s,74,mul(-50,pure));
  scored=1;
 }
 s->hud_active=0;s->hud_bailed=0;
 if(s->count>0) {
  s->last_count=s->count;s->last_base=thps1_score_base(s);
  s->last_multiplier2=thps1_score_multiplier2(s->count,half_turns);
  award=(uint32_t)mul(s->last_base,s->last_multiplier2)>>1;
  s->pending+=award;if(award>s->best_combo)s->best_combo=award;
  s->count=0;s->bank_delay=40;s->result=1;s->last_award=award;
  if(s->game_mode==2 && s->last_base!=0)s->settled_total=0;
 }
 if(scored) {
  if(!s->special) {
   s->meter=add(s->meter,signed_word(s->pending));
   if(s->meter>=3000){s->meter=0;s->special=750;}
  } else {
   s->special=add(s->special,signed_word(s->pending/5u));
   if(s->special>=751)s->special=750;
  }
 }
 s->enabled=0;return award;
}
void thps1_score_bail(Thps1Score *s) {
 int i;
 /*4ced0 clears spin before45e38, so failure callback loses rotation. */
 s->last_bail=(uint32_t)(mul(thps1_score_base(s),mul(s->count,2))/2);
 s->last_count=s->count;s->result=2;
 for(i=0;i<THPS1_SCORE_TRICKS;i++) {
  s->repetitions[i]=sub(s->repetitions[i],s->attempt_repetitions[i]);
  s->attempt_repetitions[i]=0;
 }
 if(s->hud_active){s->hud_active=0;s->hud_bailed=1;}
 s->count=0;s->enabled=0;s->meter=0;s->special=0;
}
void thps1_score_tick(Thps1Score *s) {
 if(s->meter){s->meter=sub(s->meter,2);if(s->meter<=0)s->meter=0;}
 if(s->special){s->special=sub(s->special,2);if(s->special<=0)s->special=0;}
}
void thps1_score_display_tick(Thps1Score *s) {
 if(s->game_mode!=7 && s->pending && !s->bank_delay) {
  uint32_t n=s->pending>10000?250:s->pending>2000?100:s->pending>25?25:s->pending;
  if(s->game_mode!=8)s->settled_total+=n;
  s->pending=s->game_mode==8?0:s->pending-n;
 }
 if(s->bank_delay>0)s->bank_delay--;
}
