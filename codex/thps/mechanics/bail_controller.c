/* Re-expression of THPS1 USA Rev1 8005af24..8005b6b8 control/data effects.
 * Host collision, stance/basis cleanup, sound and particles remain external. */
#include "bail_controller.h"
#include "landing_checks.h"
static int32_t word(uint32_t x) { return x<=0x7fffffffU?(int32_t)x:-1-(int32_t)~x; }
static int32_t asr(int32_t a,unsigned n) { return a>=0?(int32_t)((uint32_t)a>>n):-1-(int32_t)(~(uint32_t)a>>n); }
static void damp(Thps1Bail *b,unsigned shift,int32_t dt) {
    unsigned i;for(i=0;i<3;i++) {
        int32_t loss=asr(word((uint32_t)asr(b->velocity[i],shift)*(uint32_t)dt),8);
        b->velocity[i]=word((uint32_t)b->velocity[i]-(uint32_t)loss);
    }
}
static void zero_velocity(Thps1Bail *b) { b->velocity[0]=b->velocity[1]=b->velocity[2]=0; }
static void run(Thps1Bail *b,const ThpsAnimBank *bank,int clip,int from,int to,int continuation) {
    thps1_anim_run(&b->animation,bank,clip,from,to,continuation);
    b->events|=THPS1_BAIL_ANIMATION_CHANGED;
}
void thps1_bail_begin(Thps1Bail *b,const ThpsAnimBank *bank,int32_t reason) {
    b->reason=reason;b->active=1;b->impact_done=0;b->source_state=1;
    b->phase=thps1_landing_speed(b->velocity)>184320?1:6;
    b->animation.rate=65536;b->hidden_joint_mask|=7U;b->events=THPS1_BAIL_HIDE_BOARD;
    run(b,bank,54,0,29,-1);
}
void thps1_bail_override(Thps1Bail *b,const ThpsAnimBank *bank,int32_t phase,int32_t clip) {
    if(phase) { b->animation.rate=65536;run(b,bank,clip,0,-1,-1);b->phase=phase; }
}
void thps1_bail_step(Thps1Bail *b,const ThpsAnimBank *bank,int32_t dt,int flip_held) {
    int ground=b->source_state==0;
    b->events=0;
    if(!b->active)return;
    b->acceleration[0]=b->acceleration[2]=0;
    if((!b->reason || b->source_state==2) && b->velocity[1]<0)b->velocity[1]=0;
    switch(b->phase) {
    case 1:case 6:
        if(b->animation.frame>=15 && !b->impact_done && ground) {
            b->events|=THPS1_BAIL_IMPACT;damp(b,2,dt);b->impact_done=1;
        }
        if(b->animation.frame>=16 && ground)damp(b,5,dt);
        if(b->animation.finished && ground) {
            if(b->animation.id==46)b->phase=4;
            else if(b->phase==1)b->phase=2;
            else {run(b,bank,55,0,-1,-1);b->phase=7;}
        }
        break;
    case 2:
        if(ground)damp(b,5,dt);
        b->animation.rate=65536;run(b,bank,54,30,-1,-1);b->phase=3;
        break;
    case 3:
        if(b->animation.frame>=51 && ground)damp(b,2,dt);
        if(b->animation.finished && ground)b->phase=4;
        break;
    case 4:
        if(ground)zero_velocity(b);
        if(b->animation.frame>=112) {b->hidden_joint_mask&=~7U;b->events|=THPS1_BAIL_SHOW_BOARD;}
        if(b->animation.finished) {
            b->kick_timer=4;b->animation.rate=65536;
            if(b->crouched && flip_held)run(b,bank,8,0,26,19);
            else {b->crouched=0;run(b,bank,0,0,-1,-1);}
            b->active=0;zero_velocity(b);b->hidden_joint_mask&=~7U;
            b->events|=THPS1_BAIL_SHOW_BOARD|THPS1_BAIL_RECOVERED;
        }
        break;
    case 5:
        if(b->animation.frame>=26 && ground)damp(b,2,dt);
        else if(b->animation.frame>=34 && ground)zero_velocity(b);
        if(b->animation.finished && ground)b->phase=4;
        break;
    case 7:
        if(ground)zero_velocity(b);
        if(b->animation.finished && ground) {run(b,bank,54,99,-1,-1);b->phase=4;}
        break;
    case 8:
        if(b->animation.frame<18) {damp(b,4,dt);b->acceleration[1]=0;}
        if(b->animation.finished && ground) {run(b,bank,54,76,-1,-1);b->phase=4;}
        break;
    case 9:
        if(b->animation.frame>=29 && ground)damp(b,2,dt);
        if(b->animation.finished && ground) {run(b,bank,46,36,-1,-1);b->phase=4;}
        break;
    default:break;
    }
}
