#include "controller_n64.h"
#include "../movement/locomotion_n64.h"
#include <string.h>
static int8_t s8(int v){uint8_t x=(uint8_t)v;return x<=127?(int8_t)x:(int8_t)(-1-(255-x));}
static int run(SmN64ClimbState *s,unsigned a,const uint16_t *counts,size_t n){if(!counts || a>=n || !counts[a])return -1;smn64_anim_run(&s->anim,(uint16_t)a,counts[a],0,-1);return 1;}
static uint32_t sar(uint32_t v,unsigned n){return (v>>n)|((v&0x80000000u)?(UINT32_MAX<<(32-n)):0);}
static uint32_t random_range(SmN64ClimbState *s,uint32_t n){uint32_t v=s->random_state[0]*s->random_state[1]+s->random_state[2];s->random_state[0]=v;s->random_state[1]=(s->random_state[1]^v)+sar(v,4);s->random_state[2]+=0xefefeff0u+sar(v,3);return ((v&65535u)*n)>>16;}
void smn64_climb_seed(SmN64ClimbState *s,uint32_t seed){s->random_state[0]=seed;s->random_state[1]=0x12b9b0a1u;s->random_state[2]=0x0aa2fb3fu;}
int smn64_climb_input(SmN64ClimbState *out,int8_t x,int8_t y){
    if(!out || out->input_ramp<0 || out->input_ramp>256 || (out->dampen_left && !out->dampen_total))return -1;
    SmN64ClimbState c=*out,*s=&c;s->previous_x=s->analog_x;s->previous_y=s->analog_y;s->analog_x=s->analog_y=0;
    if(s->control_inhibit){s->input_ramp=s->d00=s->d04=0;s->aim_held=0;*out=c;return 1;}
    s->analog_x=x;s->analog_y=y;
    if(s->dampen_left){s->analog_x=s8(x-(s->dampen_left*x)/s->dampen_total);s->analog_y=s8(y-(s->dampen_left*y)/s->dampen_total);}
    if(s->d00 && !s->analog_x && !s->analog_y){s->center_pressed=s->field331=1;s->d00=0;}
    else if(s->d04 && !s->analog_x && !s->analog_y)s->d04=0;
    if((s->ceiling_class && s->d00) || (s->wall_class && s->d04))s->analog_x=s8(-s->analog_x);
    if(s->analog_x || s->analog_y){s->input_ramp+=32;if(s->input_ramp>256)s->input_ramp=256;s->input_angle=(int16_t)((s->input_base-smn64_locomotion_atan(-s->analog_x,s->analog_y)+1024)&4095);}
    else{s->input_ramp=0;s->input_angle=(int16_t)s->input_base;}
    *out=c;return 1;
}
static int cycle(SmN64ClimbState *s,uint16_t *finished,const uint16_t *counts,size_t n){
    *finished=65535;if(!s->anim.finished)return 1;
    unsigned a=s->anim.animation,next=a;int change=1;
    switch(a){
        case 19:case 29:case 30:case 52:break;
        case 53:case 54:case 55:case 56:case 227:case 234:case 281:case 181:next=19;break;
        case 50:next=random_range(s,2)?51:85;break;
        case 51:case 85:next=50;break;
        case 57:next=s->anim.direction==1?50:19;break;
        case 58:next=s->anim.direction==1?52:19;break;
        case 60:case 61:case 62:case 63:case 64:case 65:case 72:case 75:case 89:next=50;break;
        case 66:case 67:case 68:case 69:case 70:case 71:case 78:case 81:case 91:next=52;break;
        case 20:case 59:case 86:case 87:case 88:case 93:case 293:case 294:next=0;break;
        case 14:change=0;break;
        default:return -1;
    }
    if(change){if(run(s,next,counts,n)<0)return -1;*finished=(uint16_t)a;}return 1;
}
static int basis_stop(SmN64ClimbState *s,const int32_t f[3],const uint16_t *counts,size_t n){if(!smn64_climb_basis(&s->basis,f))return smn64_climb_stop(s,counts,n);return 1;}
static int action(const SmN64ClimbTransitionEnv *env,SmN64ClimbState *s,uint32_t address){return env->action?env->action(env->context,s,address):0;}
static int lost(SmN64ClimbState *s,const uint16_t *counts,size_t n){
    if(s->collision&2)return 0;
    if(s->ground_grace && --s->ground_grace)return 0;
    s->falling_origin_y=s->position[1];if(run(s,212,counts,n)<0)return -1;s->jump_variant=s->d20=s->d24=0;
    if(s->adhered){s->adhered=0;s->basis.normal[0]=s->basis.normal[2]=0;s->basis.normal[1]=-4096;const int32_t f[3]={0,0,4096};if(basis_stop(s,f,counts,n)<0)return -1;}
    s->state=4;return 1;
}
static int turn_tail(SmN64ClimbState *s,uint16_t camera){
    if(s->state&0x10){uint16_t target=s->wall_class?(uint16_t)s->input_angle:(uint16_t)((camera+s->input_angle)&4095);if(smn64_climb_turn(s,target,0)<0)return -1;}
    else if(s->state==1){if(!s->analog_x && !s->analog_y)s->turn_ticks=0;}
    else if(s->state&0x4e){if(s->d24 || (!s->analog_x && !s->analog_y))s->turn_ticks=0;else if(smn64_climb_turn(s,(uint16_t)((camera+s->input_angle)&4095),0)<0)return -1;}
    smn64_climb_turn_advance(s);return 1;
}
static int velocity_tail(SmN64ClimbState *s,uint16_t camera,int enabled){
    s->acceleration[0]=s->acceleration[2]=0;
    s->acceleration[1]=(s->adhered || s->anim.animation==277)?0:40960;
    int r=smn64_climb_target_velocity(s,s->run_ramp,enabled,camera);if(r<0)return r;
    smn64_jump_tail(&s->velocity[1],s->launch_velocity,&s->base_timer,&s->hold_timer,s->collision,s->anim.elapsed_ticks);
    return 1;
}
int smn64_climb_post_ai_tail(SmN64ClimbState *out,uint16_t camera,int enabled){
    if(!out || out->anim.elapsed_ticks<1 || out->anim.elapsed_ticks>6 || out->run_ramp<0 || out->run_ramp>16 || out->held_object)return -1;
    SmN64ClimbState s=*out;int r=turn_tail(&s,camera);if(r<0)return r;
    r=velocity_tail(&s,camera,enabled);if(r<0)return r;
    *out=s;return 1;
}
int smn64_climb_post_ai_tail_carrying(SmN64ClimbState *out,uint16_t camera,int enabled,uint32_t flags){
    if(!out || out->anim.elapsed_ticks<1 || out->anim.elapsed_ticks>6 || out->run_ramp<0 || out->run_ramp>16)return -1;
    SmN64ClimbState s=*out;int r=turn_tail(&s,camera);if(r<0)return r;
    s.acceleration[0]=s.acceleration[2]=0;
    s.acceleration[1]=(s.adhered || s.anim.animation==277)?0:40960;
    r=smn64_climb_target_velocity_carrying(&s,s.run_ramp,enabled,camera,flags);if(r<0)return r;
    smn64_jump_tail(&s.velocity[1],s.launch_velocity,&s.base_timer,&s.hold_timer,s.collision,s.anim.elapsed_ticks);
    *out=s;return 1;
}
int smn64_climb_ai(SmN64ClimbState *out,uint16_t camera,const SmN64ClimbTransitionEnv *env,SmN64ClimbTransitionEvent *event,const uint16_t *counts,size_t n){
    if(!out || !env || !event || !counts || n<300 || out->held_object)return -1;
    if(out->state!=1 && out->state!=0x10 && out->state!=0x400000 && out->state!=0x1000 && out->state!=0x2000 && out->state!=0x80000)return -1;
    SmN64ClimbState c=*out,*s=&c;SmN64ClimbTransitionEvent e;memset(&e,0,sizeof e);
    uint16_t finished;int r=cycle(s,&finished,counts,n);if(r<0)return r;
    if(s->state==0x10)s->idle_ticks=0;
    if(s->state&0x83000){r=smn64_climb_transition_step(s,finished,env,counts,n);if(r<0)return r;goto turn;}
    r=lost(s,counts,n);if(r<0)return r;if(r)goto turn;
    if(s->state==0x400000){
        s->anim.rate=131072;
        if(finished==65535){
            int frame=s->anim.frame;r=smn64_climb_jump_detach(s,counts,n);if(r<0)return r;
            if(r){if(counts[31]<=1)return -1;s->basis.yaw_delta=(int32_t)(((uint32_t)frame<<11)/(counts[31]-1));if(basis_stop(s,NULL,counts,n)<0)return -1;s->basis.yaw_delta=0;}
        }else{
            s->basis.yaw_delta=2048;if(basis_stop(s,NULL,counts,n)<0)return -1;s->basis.yaw_delta=0;
            r=smn64_climb_jump_detach(s,counts,n);if(r<0)return r;
            if(!r){if(s->analog_x || s->analog_y)s->state=0x10;else if(smn64_climb_stop(s,counts,n)<0)return -1;}
        }
        goto turn;
    }
    if(s->state==1){
        const uint32_t actions[]={0x80099444u,0x80099058u,0x8009996cu,0x8009a058u,0x80099ab8u,0x80099d54u};
        for(unsigned i=0;i<sizeof actions/sizeof actions[0];i++){r=action(env,s,actions[i]);if(r<0)return r;if(r)goto turn;}
    }else{r=action(env,s,0x80099444u);if(r<0)return r;if(r)goto turn;}
    r=smn64_climb_jump_detach(s,counts,n);if(r<0)return r;if(r)goto turn;
    if(s->state==1){
        r=smn64_climb_start_move(s,camera,1,counts,n);if(r<0)return r;if(r)goto turn;
        if(!s->aiming)s->idle_ticks=(uint16_t)(s->idle_ticks+(uint16_t)s->anim.elapsed_ticks);
        if(s->turn_ticks){s->idle_ticks=0;unsigned a=s->turn_step>0?29:30;if(s->anim.animation!=a && run(s,a,counts,n)<0)return -1;}
        else if(s->anim.animation>=27 && s->anim.animation<=30){if(run(s,19,counts,n)<0)return -1;}
        if(s->aiming && s->anim.animation!=19 && run(s,19,counts,n)<0)return -1;
    }else{
        s->idle_ticks=0;
        const uint32_t actions[]={0x8009996cu,0x80099058u,0x80099444u};
        for(unsigned i=0;i<sizeof actions/sizeof actions[0];i++){r=action(env,s,actions[i]);if(r<0)return r;if(r)goto turn;}
        r=smn64_climb_approach(s,&s->side,counts,n);if(r<0)return r;if(r)goto turn;
        if(!s->adhered){const uint32_t actions[]={0x8009a058u,0x80099ab8u,0x80099d54u};for(unsigned i=0;i<sizeof actions/sizeof actions[0];i++){r=action(env,s,actions[i]);if(r<0)return r;if(r)goto turn;}}
        r=smn64_climb_corner_begin(s,0,env,&e,counts,n);if(r<0)return r;
        if(!r){r=smn64_climb_corner_begin(s,1,env,&e,counts,n);if(r<0)return r;}
        if(!r){r=smn64_climb_ledge_begin(s,env,&e,counts,n);if(r<0)return r;}
        if(r){memset(s->velocity,0,sizeof s->velocity);goto turn;}
        if(s->aiming || (!s->analog_x && !s->analog_y)){if(smn64_climb_stop(s,counts,n)<0)return -1;goto turn;}
        if(finished==52 && !s->cf4){if(run(s,50,counts,n)<0)return -1;}
        else if((finished==51 || finished==85) && s->cf4){if(run(s,52,counts,n)<0)return -1;}
        uint16_t target=s->wall_class?(uint16_t)s->input_angle:(uint16_t)((camera+s->input_angle)&4095),diff=(uint16_t)((target-smn64_climb_heading(s))&4095);
        if(diff>=1537 && diff<=2559){unsigned a=s->anim.animation;unsigned reverse=a==52?91:((a==50 || a==51)?89:65535);if(reverse!=65535){if(run(s,reverse,counts,n)<0)return -1;memset(s->velocity,0,sizeof s->velocity);s->turn_ticks=0;s->state=0x400000;}}
    }
turn:
    if(s->cf8 && !env->camera_special && !s->field65c && s->wall_class)s->camera_restore=0;
    r=turn_tail(s,camera);if(r<0)return r;*out=c;*event=e;return 1;
}
static int marker_adapter(void *ctx,const SmN64ClimbState *s,unsigned which,int32_t out[3]){const SmN64ClimbTransitionEnv *env=ctx;return env->marker?env->marker(env->context,s,which,0,out):-3;}
static int trace_adapter(void *ctx,const SmN64ClimbQuery *q,SmN64ClimbHit *h){const SmN64ClimbTransitionEnv *env=ctx;return env->trace?env->trace(env->context,q,h):-3;}
int smn64_climb_tick_after_animation_hooked(SmN64ClimbState *out,const SmN64ClimbInput *input,const SmN64ClimbTransitionEnv *env,SmN64ClimbTransitionEvent *event,const uint16_t *counts,size_t n,uint16_t bright,uint16_t ordinary,int32_t level,SmN64ClimbPreInput hook,void *hook_context,const uint32_t *enabled){
    if(!out || !input || !env || !event || !counts || n<300 || input->elapsed_ticks<1 || input->elapsed_ticks>6 || out->run_ramp<0 || out->run_ramp>16 || out->held_object || (!out->adhered && out->state!=0x80000))return -1;
    if(input->other_actions && !env->action)return -6;
    SmN64ClimbState c=*out,*s=&c;SmN64ClimbTransitionEvent e;
    s->anim.elapsed_ticks=input->elapsed_ticks;s->anim.rate=65536;
    int r;if(!(s->state&0x83000)){s->d48=0;s->side.present=0;r=smn64_climb_physics(s,s->acceleration,s->drag,marker_adapter,trace_adapter,(void *)env,bright,ordinary,level);if(r<0)return r;}
    if(basis_stop(s,s->basis.normal[1]>=3401?s->retained_forward:NULL,counts,n)<0)return -1;
    smn64_climb_classify(s);
    if(s->adhered)s->drag[0]=s->drag[1]=s->drag[2]=1;
    else{s->drag[0]=s->drag[2]=(s->state&6)?4:1;s->drag[1]=4;}
    if(hook){r=hook(hook_context,s);if(r!=1)return r<0?r:-6;}
    s->jump_pressed=input->jump_pressed;s->jump_held=input->jump_held;s->aim_held=input->aim_held;
    r=smn64_climb_input(s,input->stick_x,input->stick_y);if(r<0)return r;
    if(s->analog_x || s->analog_y){s->run_ramp+=s->anim.elapsed_ticks;if(s->run_ramp>16)s->run_ramp=16;}
    else if(s->run_ramp){s->run_ramp-=s->anim.elapsed_ticks;if(s->run_ramp<0)s->run_ramp=0;}
    r=smn64_climb_ai(s,input->camera_yaw,env,&e,counts,n);if(r<0)return r;
    r=velocity_tail(s,input->camera_yaw,enabled?*enabled!=0:1);if(r<0)return r;
    *out=c;*event=e;return 1;
}

int smn64_climb_tick_after_animation(SmN64ClimbState *out,const SmN64ClimbInput *input,const SmN64ClimbTransitionEnv *env,SmN64ClimbTransitionEvent *event,const uint16_t *counts,size_t n,uint16_t bright,uint16_t ordinary,int32_t level){
    return smn64_climb_tick_after_animation_hooked(out,input,env,event,counts,n,bright,ordinary,level,NULL,NULL,NULL);
}

int smn64_climb_tick(SmN64ClimbState *out,const SmN64ClimbInput *input,const SmN64ClimbTransitionEnv *env,SmN64ClimbTransitionEvent *event,const uint16_t *counts,size_t n,uint16_t bright,uint16_t ordinary,int32_t level){
    if(!out || !input || input->elapsed_ticks<1 || input->elapsed_ticks>6)return -1;
    SmN64ClimbState pending=*out;pending.anim.elapsed_ticks=input->elapsed_ticks;smn64_anim_advance(&pending.anim);
    int rc=smn64_climb_tick_after_animation(&pending,input,env,event,counts,n,bright,ordinary,level);
    if(rc>0)*out=pending;
    return rc;
}
