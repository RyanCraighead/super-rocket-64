#include "ground_kick.h"
#include "ground_kernels.h"
static int eligible(int32_t clip) {return clip==0 || (clip>=5&&clip<=7);}
static int kick_clip(int32_t clip) {return clip==2||clip==3;}
static void play(Thps1GroundKick *s,int32_t clip,int32_t rate) {
 s->animation=clip;s->frame=0;s->finished=0;s->animation_rate=rate;s->animation_changed=1;
}
static void begin(Thps1GroundKick *s,int32_t stat_offset) {
 s->target_speed=thps1_ground_speed_from_total(s->effective_speed_stat+stat_offset);
 s->timer=20;s->sound_pending=1;play(s,1,0x14000);
}
void thps1_ground_kick(Thps1GroundKick *s) {
 s->acceleration_scale=0;s->animation_changed=0;s->kick_sound=0;
 if(s->state!=0)return;
 /*0x80057f24..0x80057f9c */
 if(s->finished) {
  if(s->animation==1)play(s,3,0x14000);
  else if(s->animation==3)play(s,0,0x10000);
 }
 if(s->timer>0)--s->timer;
 /*0x80058080..0x8005836c: this branch runs for push_button even if auto disabled. */
 if(s->push_button||s->auto_kick) {
  if(!s->down_and_turning) {
   if(s->timer==0) {
    if((s->slope_dot>=-1228 && s->speed>s->target_speed/2)||s->crouched) s->timer=20;
    else if(eligible(s->animation))begin(s,17);
   }
   if(kick_clip(s->animation)) {
    if(s->frame>=11 && s->sound_pending) {s->sound_pending=0;s->kick_sound=34;}
    if(s->frame>=11&&s->frame<=15&&s->speed<s->target_speed)
     s->acceleration_scale=s->push_button?8:4;
   } else if(s->timer>=11&&s->timer<=15&&s->speed<s->target_speed)
    s->acceleration_scale=4;
  }
 }
 if(s->auto_kick)return;
 /*0x80058374..0x80058574: manual branch runs after first branch. */
 if(s->push_button&&s->slope_dot>=-1228&&!s->crouched&&eligible(s->animation))begin(s,22);
 if(kick_clip(s->animation)&&s->frame>=11&&s->frame<=15&&s->speed<s->target_speed)
  s->acceleration_scale=s->push_button?8:4;
}
