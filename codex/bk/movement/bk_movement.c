/* Portable, bounded adaptation of n64decomp/banjo-kazooie (CC0).
 * Source correspondence and deliberate host boundaries: PROVENANCE.md. */
#include "bk_movement.h"
#include <math.h>
#include <string.h>

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static float lerpf(float t, float a, float b) { return t * (b - a) + a; }
static float mapf(float v, float a, float b, float c, float d) {
    return lerpf(clampf((v-a)/(b-a), 0.0f, 1.0f), c, d);
}
static float angle(float a) {
    if (!isfinite(a)) return 0.0f;
    a = fmodf(a, 360.0f);
    return a < 0.0f ? a+360.0f : a;
}
static float angle_diff(float target, float current) {
    float d = target-current;
    if (fabsf(d)>180.0f) d += d<0.0f ? 360.0f : -360.0f;
    return d;
}
static float magnitude(float x) { return isfinite(x) ? clampf(x,0.0f,1.0f) : 0.0f; }
static float speed(const BkMovement *s) {
    return sqrtf(s->velocity[0]*s->velocity[0]+s->velocity[2]*s->velocity[2]);
}
/* joy.c, controller_clampAndNormaliseJoyAxis: preserve integer truncation. */
static float axis(int input, int min, int max) {
    if (input>0) {
        input=input>max ? max : input<min ? min : input;
        input=((input-min)*80)/(max-min);
    } else if(input<0) {
        input=input< -max ? -max : input> -min ? -min : input;
        input=((input+min)*80)/(max-min);
    }
    return (1.0f/80.0f)*input;
}
BkStick bk_controller_stick(int8_t x, int8_t y) {
    BkStick out;
    float nx=axis(x,7,59), ny=axis(y,7,61);
    out.magnitude=clampf(sqrtf(nx*nx+ny*ny),0.0f,1.0f);
    /* Camera convention boundary: atan2 equivalent polar direction. The
     * original ml_acosf is a quadrant-aware approximation; it is not copied. */
    out.angle=(out.magnitude>0.0f) ? angle(atan2f(nx,ny)*(180.0f/3.141592654f)) : 0.0f;
    return out;
}
static int zone(float m) {
    if(m<=0.12f) return 0;
    if(m<=0.2f) return 1;
    if(m<=0.5f) return 2;
    if(m<=0.75f) return 3;
    return 4;
}
float bk_walk_target_speed(float m) {
    static const float marks[]={0.12f,0.2f,0.5f,0.75f,1.0f};
    static const float speeds[]={30.0f,80.0f,150.0f,225.0f,500.0f};
    int z;
    m=magnitude(m); z=zone(m);
    return z ? lerpf((m-marks[z-1])/(marks[z]-marks[z-1]),speeds[z-1],speeds[z]) : 0.0f;
}
float bk_trot_target_speed(float m) {
    m=magnitude(m);
    return m<=0.03f ? 0.0f : lerpf((m-0.03f)/(1.0f-0.03f),30.0f,700.0f);
}
static float flip_target(float m) {
    return m<=0.03f ? 0.0f : lerpf((m-0.03f)/(1.0f-0.03f),80.0f,200.0f);
}
static int trot(int a) {
    return a==BK_TROT_ENTER || a==BK_TROT_IDLE || a==BK_TROT_WALK ||
           a==BK_TROT_EXIT || a==BK_TROT_JUMP || a==BK_TROT_FALL;
}
static int walking(int a) { return a==BK_CREEP || a==BK_WALK_SLOW || a==BK_WALK || a==BK_RUN; }
static int normal_ground(int a) { return a==BK_IDLE || a==BK_LANDING || walking(a); }
static int walk_state(int z) {
    static const int states[]={BK_IDLE,BK_CREEP,BK_WALK_SLOW,BK_WALK,BK_RUN};
    return states[z];
}
static void animation(BkMovement *s,int id,float duration,float start,float end,int loop) {
    s->animation_asset=id; s->animation_duration=duration;
    s->animation_time=s->animation_previous=start;
    s->animation_end=end; s->animation_loop=loop; s->animation_stopped=0;
}
static void anim_continue(BkMovement *s,float duration,float end) {
    s->animation_duration=duration; s->animation_end=end;
    s->animation_stopped=0; s->animation_loop=0;
}
/* anctrl_isAt uses <= threshold < new time, including a loop wrap. */
static int anim_at(const BkMovement *s,float t) {
    float a=s->animation_previous,b=s->animation_time;
    if(a==b) return 0;
    return a<b ? a<=t && t<b : a<=t || t<b;
}
static void anim_tick(BkMovement *s) {
    float v;
    s->animation_previous=s->animation_time;
    if(s->animation_stopped) return;
    v=s->animation_time+BK_TICK_SECONDS/s->animation_duration;
    if(s->animation_loop) v-=(float)(int)v;
    else if(s->animation_end<v || 0.999999<(double)v) {
        v=v>s->animation_end ? s->animation_end : v;
        if(0.999999<(double)v) v=0.9999989867210388f;
        s->animation_stopped=1;
    }
    s->animation_time=v;
}
/* ml.c func_80256D0C retains its double BAD_DTOR -> float angle conversion. */
static void horizontal(BkMovement *s,float yaw,float v) {
    float radians=(float)(yaw*(3.141592654/180.0));
    s->velocity[0]=sinf(radians)*v;
    s->velocity[2]=cosf(radians)*v;
}
static void takeoff(BkMovement *s,float v,float g) {
    s->velocity[1]=v; s->gravity=g; s->grounded=0;
    s->events|=BK_EVENT_TAKEOFF;
}
static void defaults(BkMovement *s) { s->gravity=BK_GRAVITY; s->terminal_velocity=BK_TERMINAL_VELOCITY; }
static int ground_jump(uint32_t buttons) { return buttons&BK_BUTTON_Z ? BK_FLIP : BK_JUMP; }

static void enter(BkMovement *s,int a,const BkInput *in) {
    int prev=s->action;
    float oldtime=s->animation_time, oldspeed=speed(s);
    s->action=a; s->phase=0; s->elapsed=0.0f; s->timer=0.0f;
    s->attack_active=0; defaults(s);
    if(s->grounded) s->used_flap=s->used_peck=0;
    switch(a) {
    case BK_IDLE: animation(s,0x6f,5.5f,0,1,1); s->target_speed=0; break;
    case BK_CREEP: animation(s,2,0.43f,prev==BK_WALK_SLOW ? oldtime:0,1,1); break;
    case BK_WALK_SLOW: animation(s,3,0.43f,prev==BK_WALK ? oldtime:0,1,1); break;
    case BK_WALK: case BK_RUN:
        animation(s,0xc,0.66f,walking(prev)?oldtime:0,1,1); s->timer=0.3f; break;
    case BK_CROUCH:
        animation(s,1,0.5f,0,1,0); s->saved_speed=oldspeed; s->timer=0.7f;
        if(oldspeed>0.0f) s->target_yaw=angle(atan2f(s->velocity[0],s->velocity[2])*(180.0f/3.141592654f));
        break;
    case BK_JUMP: case BK_TROT_JUMP:
        if(in->stick_magnitude>0) s->ideal_yaw=in->world_yaw;
        s->target_yaw=s->ideal_yaw;
        s->target_speed=a==BK_TROT_JUMP ? bk_trot_target_speed(in->stick_magnitude):bk_walk_target_speed(in->stick_magnitude);
        horizontal(s,s->target_yaw,s->target_speed);
        if(a==BK_TROT_JUMP) {
            animation(s,0x27,1.4f,0.2f,0.4002f,0); takeoff(s,693.5f,-1200.0f);
        } else {
            animation(s,8,1.9f,0.3f,0.5042f,0); takeoff(s,710.0f,-1350.0f);
        }
        break;
    case BK_FALL: case BK_TROT_FALL:
        animation(s,a==BK_TROT_FALL?0x27:0xb0,a==BK_TROT_FALL?10.0f:0.38f,a==BK_TROT_FALL?0.4653f:0.0f,a==BK_TROT_FALL?0.4653f:1.0f,a!=BK_TROT_FALL);
        s->grounded=0;
        if(a==BK_TROT_FALL) {
            s->animation_stopped=1; s->animation_duration=1.4f;
            s->target_yaw=s->ideal_yaw;
            s->target_speed=bk_trot_target_speed(in->stick_magnitude);
            horizontal(s,s->target_yaw,s->target_speed);
        }
        break;
    case BK_LANDING:
        s->target_speed=0;
        /* The host lacks the original 90/130-unit ground-proximity query.
         * Complete the original jump landing subrange at contact instead. */
        animation(s,8,1.4f,0.6667f,1,0); break;
    case BK_TROT_ENTER: animation(s,0x16,1,0,1,0); s->target_speed=0; break;
    case BK_TROT_IDLE: animation(s,0x26,1.2f,0,1,1); s->target_speed=0; break;
    case BK_TROT_WALK: animation(s,0x15,0.53f,0,1,1); break;
    case BK_TROT_EXIT: animation(s,7,0.6f,0,1,0); s->target_speed=0; break;
    case BK_FLIP:
        animation(s,0x4b,2.3f,0,0.7866f,0); break;
    case BK_FLAP:
        animation(s,0x18,0.3f,0,1,0);
        if(in->stick_magnitude>0) s->ideal_yaw=in->world_yaw;
        s->target_yaw=s->ideal_yaw; s->target_speed=bk_walk_target_speed(in->stick_magnitude);
        horizontal(s,s->target_yaw,s->target_speed);
        s->velocity[1]=0; s->gravity=-1100; s->used_flap=1; s->flap_count=0;
        s->grounded=0; s->timer=2.5f; break;
    case BK_CLAW:
        animation(s,5,1.3f,0,1,0); s->target_yaw=s->ideal_yaw; s->target_speed=160; break;
    case BK_ROLL:
        animation(s,0x4f,0.9f,0,1,0); s->target_yaw=s->ideal_yaw;
        horizontal(s,s->target_yaw,s->target_speed); s->target_speed=600; s->attack_active=1; break;
    case BK_BARGE:
        animation(s,0x1c,1,0,0.375f,0); s->target_yaw=s->ideal_yaw;
        s->target_speed*=0.3; horizontal(s,s->target_yaw,s->target_speed); s->barge_released=0; break;
    case BK_PECK:
        animation(s,0x1a,0.2f,0,1,0); s->from_trot=trot(prev);
        s->gravity=-1400; s->velocity[1]=120; s->attack_active=1; s->grounded=0; break;
    case BK_BUSTER:
        animation(s,0x1d,1.02f,0,0.35f,0); s->gravity=0; s->target_speed=0;
        memset(s->velocity,0,sizeof(s->velocity)); s->timer=0.0001f; s->grounded=0; break;
    default: s->action=BK_IDLE; animation(s,0x6f,5.5f,0,1,1); s->target_speed=0; break;
    }
}
void bk_movement_init(BkMovement *s,float yaw,int grounded) {
    BkInput in={0};
    if(!s) return;
    memset(s,0,sizeof(*s)); s->grounded=!!grounded;
    s->yaw=s->ideal_yaw=s->target_yaw=angle(yaw);
    enter(s,grounded?BK_IDLE:BK_FALL,&in);
}

/* Source yaw.c __yaw_update_limited, with state-specific bounded values. */
static void yaw_limited(BkMovement *s,float limit,float percent) {
    float d=angle_diff(s->ideal_yaw,s->yaw),max=limit*BK_TICK_SECONDS;
    float v=d*percent*BK_TICK_SECONDS;
    v=v<0 ? clampf(v,-max,-0.1f):clampf(v,0.1f,max);
    s->yaw=angle(fabsf(v)<=fabsf(d) ? s->yaw+v:s->ideal_yaw);
}
static void update_yaw(BkMovement *s,const BkInput *in) {
    int a=s->action;
    if(a==BK_FLIP && s->phase==0) return;
    if(a==BK_TROT_ENTER || a==BK_TROT_EXIT) return;
    if(normal_ground(a) || a==BK_CROUCH || a==BK_FLAP || a==BK_PECK ||
       a==BK_TROT_IDLE || a==BK_TROT_WALK) {
        if(in->stick_magnitude>0.0f && (a!=BK_CROUCH || fabsf(angle_diff(in->world_yaw,s->ideal_yaw))>=8))
            s->ideal_yaw=in->world_yaw;
    }
    if(a==BK_CROUCH) yaw_limited(s,350,14);
    else if(a==BK_PECK) yaw_limited(s,1200,10);
    else if(a==BK_FLIP && s->phase<4) {
        float d=angle_diff(s->ideal_yaw,s->yaw),step=s->saved_speed*BK_TICK_SECONDS;
        s->yaw=angle(fabsf(d)<=step?s->ideal_yaw:s->yaw+(d<0?-step:step));
    } else yaw_limited(s,700,7.5f);
}
static void scale_animation(BkMovement *s) {
    float v=speed(s),d=s->animation_duration;
    /* ba/anim.c uses UNCLAMPED mapRange then duration clamp .3..1.5. */
    switch(s->action) {
    case BK_CREEP: d=lerpf((v-30)/(80-30),1.8f,1.2f); break;
    case BK_WALK_SLOW: d=lerpf((v-80)/(150-80),1.3f,0.6f); break;
    case BK_WALK: d=lerpf((v-150)/(225-150),0.92f,0.58f); break;
    case BK_RUN: d=lerpf((v-225)/(500-225),0.54f,0.44f); break;
    case BK_TROT_WALK: d=lerpf((v-30)/(700-30),0.56f,0.34f); break;
    default: return;
    }
    s->animation_duration=clampf(d,0.3f,1.5f);
}
static int ground_inputs(BkMovement *s,const BkInput *in,uint32_t pressed,int next) {
    if(!s->grounded) next=BK_FALL;
    if(in->buttons&BK_BUTTON_Z) next=BK_CROUCH;
    if(pressed&BK_BUTTON_B) next=walking(s->action) && s->target_speed>225 ? BK_ROLL:BK_CLAW;
    if(pressed&BK_BUTTON_A) next=ground_jump(in->buttons);
    return next;
}
static int trot_inputs(BkMovement *s,const BkInput *in,uint32_t pressed,int next) {
    if(in->stick_magnitude>0.03f) next=BK_TROT_WALK;
    if(!s->grounded) next=BK_TROT_FALL;
    if(!(in->buttons&BK_BUTTON_Z)) next=BK_TROT_EXIT;
    if(pressed&BK_BUTTON_A) next=BK_TROT_JUMP;
    return next;
}
static void flip_land(BkMovement *s) {
    animation(s,0x4b,2.2f,0.8566f,1,0); s->target_speed=0;
    memset(s->velocity,0,sizeof(s->velocity)); s->phase=4;
    s->events|=BK_EVENT_LANDED;
}
static int update_state(BkMovement *s,const BkInput *in,uint32_t pressed) {
    int a=s->action,next=0,z=zone(in->stick_magnitude);
    float t=s->animation_time;
    s->elapsed+=BK_TICK_SECONDS;
    if(normal_ground(a)) {
        s->used_flap=s->used_peck=0;
        if(walking(a)) {
            s->target_speed=bk_walk_target_speed(in->stick_magnitude);
            s->timer=fmaxf(0,s->timer-BK_TICK_SECONDS);
            if(a==BK_CREEP || a==BK_WALK_SLOW) {
                if(z) next=walk_state(z);
                else if(speed(s)<=(a==BK_CREEP?1.0f:3.0f)) next=BK_IDLE;
            } else if(a==BK_WALK) {
                if(z==4) next=BK_RUN;
                else if(z<3 && speed(s)<=150 && s->timer==0) next=BK_WALK_SLOW;
            } else {
                if(z==0 && speed(s)<=18) next=BK_IDLE;
                if((z==1 || z==2) && speed(s)<=150) next=BK_WALK_SLOW;
                if(z==3 && speed(s)<=225 && s->timer==0) next=BK_WALK;
            }
        } else {
            s->target_speed=0;
            if(z) next=walk_state(z);
            if(a==BK_LANDING && s->animation_stopped) next=BK_IDLE;
        }
        return ground_inputs(s,in,pressed,next);
    }
    switch(a) {
    case BK_CROUCH:
        s->timer=fmaxf(0,s->timer-BK_TICK_SECONDS);
        s->target_speed=mapf(s->timer,0,0.3f,0,s->saved_speed);
        if(s->animation_stopped && s->animation_asset==1) animation(s,0x10c,0.5f,0,1,1);
        if(!s->grounded) next=BK_FALL;
        if(!(in->buttons&BK_BUTTON_Z)) {
            if(s->elapsed>=0.2f) next=BK_IDLE;
            if(pressed&BK_BUTTON_B) next=BK_CLAW;
            if(pressed&BK_BUTTON_A) next=BK_JUMP;
        } else {
            if(pressed&BK_BUTTON_C_LEFT) next=BK_TROT_ENTER;
            if(pressed&BK_BUTTON_A) next=BK_FLIP;
            if(pressed&BK_BUTTON_B) next=BK_BARGE;
        }
        break;
    case BK_TROT_ENTER:
        if(s->animation_stopped) next=BK_TROT_IDLE;
        if(s->animation_time>0.5f) next=trot_inputs(s,in,pressed,next);
        break;
    case BK_TROT_IDLE:
        next=trot_inputs(s,in,pressed,0); break;
    case BK_TROT_WALK:
        s->target_speed=bk_trot_target_speed(in->stick_magnitude);
        if(in->stick_magnitude<=0.03f && speed(s)<=1) next=BK_TROT_IDLE;
        if(!s->grounded) next=BK_TROT_FALL;
        if(!(in->buttons&BK_BUTTON_Z)) next=BK_TROT_EXIT;
        if(pressed&BK_BUTTON_A) next=BK_TROT_JUMP;
        break;
    case BK_TROT_EXIT:
        if(s->animation_stopped) next=BK_IDLE;
        if(!s->grounded) next=BK_FALL;
        break;
    case BK_JUMP: case BK_FALL: case BK_TROT_JUMP: case BK_TROT_FALL:
        s->target_speed=trot(a)?bk_trot_target_speed(in->stick_magnitude):bk_walk_target_speed(in->stick_magnitude);
        if(!(in->buttons&BK_BUTTON_A) && s->velocity[1]>0) s->gravity=BK_GRAVITY;
        if(s->animation_stopped && s->phase==0 && a==BK_JUMP) { anim_continue(s,4,0.6667f); s->phase=1; }
        if(s->animation_stopped && s->phase==0 && a==BK_TROT_JUMP) { anim_continue(s,10,0.4653f); s->phase=1; }
        if(!trot(a) && !s->used_flap && !s->used_peck && (pressed&BK_BUTTON_A)) next=BK_FLAP;
        if(!s->used_flap && !s->used_peck && (pressed&BK_BUTTON_B)) next=BK_PECK;
        if(!trot(a) && (pressed&BK_BUTTON_Z)) next=BK_BUSTER;
        if(s->grounded) {
            s->events|=BK_EVENT_LANDED;
            next=trot(a)?((in->buttons&BK_BUTTON_Z)?BK_TROT_IDLE:BK_TROT_EXIT):BK_LANDING;
            if(trot(a) && (pressed&BK_BUTTON_A)) next=BK_TROT_JUMP;
        }
        break;
    case BK_FLIP:
        if(s->phase==0 && anim_at(s,0.1837f)) {
            if(in->stick_magnitude>0) {
                s->ideal_yaw=in->world_yaw;
                s->saved_speed=fabsf(angle_diff(s->ideal_yaw,s->yaw));
                s->target_yaw=s->ideal_yaw; s->target_speed=200;
                horizontal(s,s->target_yaw,200);
            } else { s->target_speed=0; s->saved_speed=0; }
            takeoff(s,920,-1200); s->terminal_velocity=-533.3f;
            s->animation_duration=1.9f; s->phase=1;
        } else if(s->phase==1) {
            s->target_speed=flip_target(in->stick_magnitude);
            if(s->animation_stopped) { animation(s,0x4c,0.13f,0,1,1); s->phase=2; }
            if(pressed&BK_BUTTON_Z) next=BK_BUSTER;
            /* Original only checks stable in hold/exit; early ceiling/floor
             * contacts otherwise trap the wind-up animation. Wait for hold. */
        } else if(s->phase==2 || s->phase==3) {
            if(s->phase==2 && !(in->buttons&BK_BUTTON_A)) {
                animation(s,0x61,0.8f,0,1,0); s->terminal_velocity=BK_TERMINAL_VELOCITY; s->phase=3;
            }
            if(s->grounded) flip_land(s);
            else if(pressed&BK_BUTTON_Z) next=BK_BUSTER;
        } else if(s->phase==4 && s->animation_stopped) next=BK_WALK_SLOW;
        break;
    case BK_FLAP:
        s->target_speed=bk_walk_target_speed(in->stick_magnitude);
        s->timer=fmaxf(0,s->timer-BK_TICK_SECONDS);
        if(s->phase==0 && anim_at(s,0.9f)) {
            animation(s,0x17,0.15f,0,1,1);
            s->velocity[1]=280; s->gravity=-1100; s->terminal_velocity=-399.9f; s->phase=1;
        } else if(s->phase>=1 && s->phase<=3) {
            static const float durations[]={0.15f,0.2f,0.27f,0.38f,0.4f,0.7f};
            int old_phase=s->phase;
            if(anim_at(s,0.9f)) ++s->flap_count;
            s->animation_duration=durations[s->flap_count<5?s->flap_count:5];
            if(old_phase==1 && s->elapsed>=0.67f) s->phase=2;
            else if(old_phase==2 && s->flap_count==4) s->phase=3;
            if(old_phase>=2 && !(in->buttons&BK_BUTTON_A)) {
                defaults(s); s->animation_duration=1; s->phase=4;
            } else if(old_phase==3) s->target_speed*=0.35;
        }
        if(s->timer==0) next=BK_FALL;
        if(pressed&BK_BUTTON_Z) next=BK_BUSTER;
        if(s->grounded) { s->events|=BK_EVENT_LANDED; next=BK_WALK_SLOW; }
        break;
    case BK_CLAW:
        s->attack_active=anim_at(s,0.1488f) || (0.04879999999999998<t && t<0.2488) ||
            anim_at(s,0.3288f) || (0.22879999999999998<t && t<0.42879999999999998) ||
            anim_at(s,0.5788f) || (0.4788<t && t<0.6788);
        if(anim_at(s,0.5788f)) s->target_speed=0;
        if(s->animation_stopped) next=BK_IDLE;
        if(!s->grounded) next=BK_FALL;
        if(pressed&BK_BUTTON_A) next=ground_jump(in->buttons);
        break;
    case BK_ROLL:
        if(s->phase<2) ++s->phase;
        else if(s->phase==2 && anim_at(s,0.8011f)) {
            s->animation_duration=2.5f; s->target_speed=0; s->attack_active=0; s->phase=3;
        }
        if(s->animation_stopped) next=BK_IDLE;
        if(pressed&BK_BUTTON_A) next=ground_jump(in->buttons);
        if(t>0.6f && !s->grounded) next=BK_FALL;
        break;
    case BK_BARGE:
        if(!(in->buttons&BK_BUTTON_B)) s->barge_released=1;
        if(s->phase==0 && s->animation_stopped) {
            s->saved_speed=s->barge_released?500.0f:850.0f; s->phase=1; s->timer=0.01f;
        } else if(s->phase==1) {
            s->timer=fmaxf(0,s->timer-BK_TICK_SECONDS);
            if(s->timer==0) {
                anim_continue(s,1,0.565f); s->target_speed=s->saved_speed;
                horizontal(s,s->ideal_yaw,s->target_speed); s->phase=2; s->attack_active=1;
            }
        } else if(s->phase==2) {
            s->target_speed=s->saved_speed;
            if(s->animation_stopped) { anim_continue(s,2,0.6f); s->timer=0.1f; s->phase=3; }
        } else if(s->phase==3) {
            s->timer=fmaxf(0,s->timer-BK_TICK_SECONDS);
            /* Source BA_FLAG_C is latched at launch, not current held B. */
            if(s->saved_speed<=500 || s->timer==0) s->saved_speed-=80;
            s->target_speed=s->saved_speed;
            if(s->saved_speed<200) { anim_continue(s,1.5f,1); s->phase=4; }
        } else if(s->phase==4) {
            if(anim_at(s,0.7f)) { s->saved_speed=0; s->attack_active=0; }
            s->target_speed=s->saved_speed;
            if(!s->grounded) next=BK_FALL;
            if(anim_at(s,0.9193f)) next=BK_LANDING;
        }
        break;
    case BK_PECK:
        s->target_speed=bk_walk_target_speed(in->stick_magnitude);
        if(s->from_trot) s->target_speed*=0.1;
        if(s->phase==0 && anim_at(s,0.9126f)) {
            animation(s,0x19,0.35f,0,1,1); s->timer=0.5f; s->phase=1;
        } else if(s->phase==1) {
            if(anim_at(s,0.1621f) || anim_at(s,0.7f)) { s->velocity[1]=120; s->used_peck=1; }
            s->timer-=BK_TICK_SECONDS;
            if(s->timer<0) {
                /* Reversed original .2s exit represented by decreasing
                 * normalized phase (handled by anim_tick below). */
                animation(s,0x1a,0.2f,0.9999989867210388f,0,0); s->phase=2;
            }
        } else if(s->phase==2 && s->animation_stopped) next=BK_FALL;
        if(s->grounded) { s->events|=BK_EVENT_LANDED; next=BK_IDLE; }
        break;
    case BK_BUSTER:
        if(s->phase==0 && s->animation_stopped) { s->animation_duration=0.4f; s->phase=1; }
        else if(s->phase==1) {
            s->timer-=BK_TICK_SECONDS;
            if(s->timer<=0) { takeoff(s,2300,-20000); s->terminal_velocity=-5000; s->attack_active=1; s->phase=2; }
        } else if(s->phase==2 && s->grounded) {
            memset(s->velocity,0,sizeof(s->velocity)); s->gravity=0; s->target_speed=0;
            s->attack_active=2; s->events|=BK_EVENT_BUSTER_IMPACT|BK_EVENT_LANDED; s->timer=0.09f; s->phase=3;
        } else if(s->phase==3) {
            s->attack_active=0; s->timer-=BK_TICK_SECONDS;
            if(s->timer<=0) { takeoff(s,730,-2110); anim_continue(s,1.9f,0.7299f); s->phase=4; }
        } else if(s->phase==4) {
            s->target_speed=bk_walk_target_speed(in->stick_magnitude);
            if(s->animation_stopped) anim_continue(s,15,0.74f);
            if(s->grounded) { s->events|=BK_EVENT_LANDED; next=BK_LANDING; }
        }
        break;
    default: next=BK_IDLE; break;
    }
    return next;
}

/* __baphysics_update_normal flat-floor branch. Separate multiply/subtract
 * preserves original rounding: (target*k - actual*k), not (target-actual)*k.
 * Material 1=.07 in air; 2=.29 normal ground; 4=.05 ice (.24 in trot). */
static void physics(BkMovement *s,const BkInput *in,BkMotion *out) {
    static const float factors[]={0.0f,0.07f,0.29f,0.15f,0.05f};
    float k,r,tx,tz,change;
    int material=s->grounded ? in->floor_type:1;
    int a=s->action;
    if(material<1 || material>4) material=2;
    k=(material==4 && trot(a)) ? 0.24f:factors[material];
    if(normal_ground(a) || a==BK_FLAP || a==BK_TROT_IDLE || a==BK_TROT_WALK || a==BK_TROT_ENTER)
        s->target_yaw=s->ideal_yaw;
    else if((a==BK_JUMP || a==BK_FALL || a==BK_TROT_JUMP || a==BK_TROT_FALL ||
             a==BK_PECK || a==BK_BUSTER || (a==BK_FLIP && s->phase>0 && s->phase<4)) && in->stick_magnitude>0)
        s->target_yaw=in->world_yaw;
    r=(float)(s->target_yaw*(3.141592654/180.0));
    tx=sinf(r)*s->target_speed; tz=cosf(r)*s->target_speed;
    change=tx*k-s->velocity[0]*k;
    change*=(float)(BK_TICK_SECONDS/0.0333333);
    s->velocity[0]+=change;
    change=tz*k-s->velocity[2]*k;
    change*=(float)(BK_TICK_SECONDS/0.0333333);
    s->velocity[2]+=change;
    out->delta[0]=s->velocity[0]*BK_TICK_SECONDS;
    out->delta[2]=s->velocity[2]*BK_TICK_SECONDS;
    if(fabsf(s->velocity[0])<0.0001) s->velocity[0]=0;
    if(fabsf(s->velocity[2])<0.0001) s->velocity[2]=0;
    s->velocity[1]=s->velocity[1]+BK_TICK_SECONDS*s->gravity;
    if(s->velocity[1]<s->terminal_velocity) s->velocity[1]=s->terminal_velocity;
    out->delta[1]=s->velocity[1]*BK_TICK_SECONDS;
}
static uint32_t model_flags(const BkMovement *s) {
    if(trot(s->action)) return BK_MODEL_KAZOOIE_UPPER|BK_MODEL_KAZOOIE_FEET|BK_MODEL_KAZOOIE_DIRECTION;
    if(s->action==BK_FLIP || s->action==BK_FLAP || s->action==BK_PECK || s->action==BK_BARGE || s->action==BK_BUSTER)
        return BK_MODEL_KAZOOIE_UPPER;
    return 0;
}
void bk_movement_tick(BkMovement *s,const BkInput *input,BkMotion *out) {
    BkInput in;
    uint32_t pressed;
    int next;
    if(!out) return;
    memset(out,0,sizeof(*out));
    if(!s || !input) return;
    in=*input; in.stick_magnitude=magnitude(in.stick_magnitude);
    if(!isfinite(in.world_yaw)) in.stick_magnitude=0;
    in.world_yaw=angle(in.world_yaw);
    pressed=in.buttons&~s->previous_buttons;
    s->previous_buttons=in.buttons; s->events=0;
    next=update_state(s,&in,pressed);
    if(next && next!=s->action) enter(s,next,&in);
    physics(s,&in,out);
    update_yaw(s,&in);
    scale_animation(s);
    if(s->action==BK_PECK && s->phase==2 && !s->animation_stopped) {
        s->animation_previous=s->animation_time;
        s->animation_time-=BK_TICK_SECONDS/s->animation_duration;
        if(s->animation_time<0) { s->animation_time=0; s->animation_stopped=1; }
    } else anim_tick(s);
    ++s->ticks;
    memcpy(out->velocity,s->velocity,sizeof(out->velocity));
    out->yaw=s->yaw; out->animation_asset=s->animation_asset;
    out->animation_time=s->animation_time; out->action=s->action;
    out->airborne=!s->grounded; out->attack_active=s->attack_active;
    out->model_flags=model_flags(s); out->events=s->events; out->ticks=s->ticks;
}
void bk_movement_contact(BkMovement *s,int on_floor,int hit_ceiling,int blocked_x,int blocked_z) {
    if(!s) return;
    if(hit_ceiling && s->velocity[1]>0) s->velocity[1]=0;
    s->grounded=!!on_floor && s->velocity[1]<=0;
    if(s->grounded && s->velocity[1]<0) s->velocity[1]=-1.0f; /* code_C4B0.c */
    if(blocked_x) s->velocity[0]=0;
    if(blocked_z) s->velocity[2]=0;
}
const char *bk_action_name(int a) {
    switch(a) {
    case BK_IDLE:return "Idle"; case BK_CREEP:return "Creep"; case BK_WALK_SLOW:return "Slow walk";
    case BK_WALK:return "Walk"; case BK_RUN:return "Run"; case BK_JUMP:return "Jump";
    case BK_CROUCH:return "Crouch"; case BK_TROT_ENTER:return "Talon trot enter";
    case BK_TROT_IDLE:return "Talon trot idle"; case BK_TROT_WALK:return "Talon trot";
    case BK_TROT_EXIT:return "Talon trot exit"; case BK_TROT_JUMP:return "Talon trot jump";
    case BK_TROT_FALL:return "Talon trot fall"; case BK_FLIP:return "Flap flip";
    case BK_FLAP:return "Feathery flap"; case BK_CLAW:return "Claw swipe";
    case BK_ROLL:return "Roll"; case BK_BARGE:return "Beak barge";
    case BK_PECK:return "Rat-a-tat rap"; case BK_BUSTER:return "Beak buster";
    case BK_LANDING:return "Landing"; case BK_FALL:return "Fall"; default:return "Unknown";
    }
}
