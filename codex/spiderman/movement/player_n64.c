#include "player_n64.h"
#include <limits.h>
#include <string.h>

static int32_t signed32(uint32_t x) {
    return x <= INT32_MAX ? (int32_t)x : -1-(int32_t)(UINT32_MAX-x);
}
static uint32_t sar(uint32_t x,unsigned n) {
    return (x>>n)|((x&0x80000000u)?(UINT32_MAX<<(32-n)):0u);
}
static uint32_t random_range(SmN64Player *s,uint32_t n) {
    uint32_t v=s->random_state[0]*s->random_state[1]+s->random_state[2];
    s->random_state[0]=v;
    s->random_state[1]=(s->random_state[1]^v)+sar(v,4);
    s->random_state[2]+=0xefefeff0u+sar(v,3);
    return sar((v&65535u)*n,16);
}
static int play(SmN64Player *s,uint16_t clip,int from,const uint16_t *counts,size_t n) {
    if(clip>=n || !counts[clip])return -1;
    smn64_anim_run(&s->motion.anim,clip,counts[clip],from,-1);return 1;
}
static int stand(SmN64Player *s,const uint16_t *counts,size_t n) {
    uint16_t clip=0;
    if(s->motion.anim.animation==21) {
        uint16_t f=(uint16_t)s->motion.anim.frame;
        clip=(f>=4 && f<=17)?((f>=10 && f<=14)?12:13):11;
    }
    s->motion.state=1;
    return play(s,clip,0,counts,n);
}
static int jump_request(SmN64Player *s,const uint16_t *counts,size_t n) {
    int running=(s->motion.state&0x10u)!=0;
    if(play(s,running?223:210,running?5:4,counts,n)<0)return -1;
    s->launch_velocity=-245760;s->jump_variant=running;s->motion.state=0x40;
    if(s->collision&1u)s->motion.run_ramp=0;
    return 1;
}
static int lost_ground(SmN64Player *s,const uint16_t *counts,size_t n,int *holding,SmN64PlayerRoute route,void *context) {
    if(s->collision&2u)return 0;
    if(s->ground_grace && --s->ground_grace)return 0;
    if(*holding>=0){
        int rc;if(!route)return -1;rc=route(context,s,SMN64_ROUTE_CARRY_DROP);
        if(rc)return rc<0?rc:-1;
        *holding=-1;
    }
    s->falling_origin_y=s->position[1];s->jump_variant=0;s->motion.state=4;
    return play(s,212,0,counts,n);
}
static int landing(SmN64Player *s,const uint16_t *counts,size_t n) {
    uint16_t a=s->motion.anim.animation,clip=213;
    int active=s->motion.analog_x || s->motion.analog_y;
    if(a==232)clip=active?236:237;
    else if(a==225 || (s->jump_variant && (a==226 || a==228 || a==233 || a==235)))clip=active?229:230;
    else s->jump_variant=0;
    if(!active && (clip==237 || clip==230))s->jump_variant=0;
    s->motion.state=8;
    return play(s,clip,0,counts,n);
}

int smn64_player_init(SmN64Player *s,const int32_t pos[3],uint16_t yaw,uint32_t seed,
                     const uint16_t *counts,size_t count) {
    if(!s || !pos || !counts || count<300)return -1;
    memset(s,0,sizeof(*s));memcpy(s->position,pos,sizeof(s->position));
    s->motion.state=1;s->motion.anim.rate=65536;s->motion.anim.elapsed_ticks=2;
    s->motion.forward_x=0;s->motion.forward_z=4096;
    s->motion.yaw_delta=yaw&4095;smn64_locomotion_basis_begin(&s->motion);
    s->motion.yaw_delta=0;
    s->motion.yaw=(int16_t)(yaw&4095);
    s->random_state[0]=seed;s->random_state[1]=0x12b9b0a1u;s->random_state[2]=0x0aa2fb3fu;
    s->drag[0]=1;s->drag[1]=4;s->drag[2]=1;s->collision=2;s->ground_grace=4;
    return play(s,0,0,counts,count);
}

int smn64_player_prepare_after_animation(SmN64Player *s,const SmN64Input *input) {
    if(!s || !input || s->awaiting_contact || s->suspended ||
       input->elapsed_ticks<1 || input->elapsed_ticks>6)return -1;
    s->motion.anim.elapsed_ticks=input->elapsed_ticks;
    s->previous_velocity_y=s->motion.velocity_y;
    s->ticks+=input->elapsed_ticks;s->awaiting_contact=1;return 1;
}
int smn64_player_prepare(SmN64Player *s,const SmN64Input *input) {
    if(!s || !input || s->awaiting_contact || s->suspended || input->elapsed_ticks<1 || input->elapsed_ticks>6)return -1;
    s->motion.anim.elapsed_ticks=input->elapsed_ticks;
    smn64_anim_advance(&s->motion.anim);
    return smn64_player_prepare_after_animation(s,input);
}
int smn64_player_begin(SmN64Player *s,const SmN64Input *input,SmN64MotionRequest *out) {
    if(!out)return -1;
    int rc=smn64_player_prepare(s,input);if(rc<0)return rc;
    int32_t v[3]={s->motion.velocity_x,s->motion.velocity_y,s->motion.velocity_z};
    const int32_t a[3]={0,40960,0};
    smn64_free_motion(v,a,s->drag,input->elapsed_ticks,out->displacement);
    s->motion.velocity_x=v[0];s->motion.velocity_y=v[1];s->motion.velocity_z=v[2];
    for(unsigned i=0;i<3;++i) {
        out->from[i]=s->position[i];out->velocity[i]=v[i];
        out->proposed[i]=signed32((uint32_t)s->position[i]+(uint32_t)out->displacement[i]);
    }
    out->body_to_floor=96*4096;
    return 1;
}

static int basis_update(SmN64Locomotion *s,SmN64PlayerBasis replace,void *context) {
    if(replace) {int rc=replace(context,s);return rc==1?1:rc<0?rc:-5;}
    smn64_locomotion_basis_begin(s);return 1;
}
static int finish_with_hooks(SmN64Player *out,const SmN64Input *input,const SmN64Contact *contact,
                       const uint16_t *counts,size_t count,SmN64PlayerRoute route,SmN64PlayerBasis basis,void *context,int holding) {
    if(!out || !input || !contact || !counts || count<300 || !out->awaiting_contact ||
       input->elapsed_ticks<1 || input->elapsed_ticks>6 ||
       input->elapsed_ticks!=(uint16_t)out->motion.anim.elapsed_ticks)return -1;
    if(contact->unsupported) {out->awaiting_contact=0;out->suspended=-2;return -2;}
    SmN64Player c=*out,*s=&c;SmN64Locomotion *m=&s->motion;
    s->awaiting_contact=0;memcpy(s->position,contact->position,sizeof(s->position));
    s->collision=(uint16_t)((holding>=0?(s->collision&0x40u):0)|(contact->wall?1:0)|(contact->grounded?2:0)|(contact->ceiling?0x100:0));
    if(contact->grounded) {s->ground_grace=4;m->velocity_y=0;}
    if(contact->ceiling) m->velocity_y=0;
    /* Drag belongs to the entry state, before the current frame changes it. */
    s->drag[0]=s->drag[2]=(m->state&6u)?4:1;s->drag[1]=4;
    m->anim.rate=65536;
    int turn_done=0,motion_input_enabled=1;
    int rc=basis_update(m,basis,context);if(rc<0)goto tail;
    smn64_locomotion_input(m,input->stick_x,input->stick_y);
    smn64_locomotion_ramp(m);
    uint32_t entry=m->state;
    uint16_t finished_clip=65535;
    /* Original runtime sorts this authored successor table before use. These
     * are the verified reachable entries for the bounded air/landing slice. */
    if(entry==1||entry==0x10||entry==0x400000){
        int completed=smn64_locomotion_grounded_successor(m,counts,count);if(completed<0){rc=completed;goto tail;}
        finished_clip=(uint16_t)completed;
    }else if(m->anim.finished) {
        uint16_t a=m->anim.animation,next=65535;
        switch(a) {
            case 212:case 216:case 219:case 222:case 225:case 276:case 278:next=226;break;
            case 213:case 230:case 237:next=0;break;
            case 177:case 226:next=228;break;
            case 178:next=180;break;case 180:next=0;break;
            case 228:next=228;break;
            case 229:case 236:next=21;break;
            case 231:next=232;break;
            case 232:next=233;break;
            case 233:next=235;break;
            case 235:next=235;break;
            default:break;
        }
        if(next!=65535) {finished_clip=a;rc=play(s,next,0,counts,count);if(rc<0)goto tail;}
    }
    /* These calls are at the original dispatcher boundaries, rather than a
     * once-per-frame preflight which would change simultaneous-input priority. */
#define ROUTE(stage) do { if(route) { int routed=route(context,s,stage); \
    if(routed<0){rc=routed;goto tail;} \
    if(routed>0){*out=*s;return 2;} } } while(0)
    if(entry==1 || entry==0x10 || entry==0x400000) {
        if(entry==0x10&&holding>=0&&(s->collision&0x40u)){
            if(!route){rc=-1;goto tail;}
            rc=route(context,s,SMN64_ROUTE_CARRY_SMASH);if(rc){if(rc>0)rc=-1;goto tail;}
            holding=-1;
        }
        {int lost=lost_ground(s,counts,count,&holding,route,context);if(lost){rc=lost;goto tail;}}
        if(entry==1) ROUTE(SMN64_ROUTE_IDLE_BEFORE_JUMP);
        else if(entry==0x10) ROUTE(SMN64_ROUTE_RUN_BEFORE_JUMP);
        if(input->jump_pressed && entry==0x400000) {
            if(counts[31]<=1 || (finished_clip==65535 ? (m->anim.animation!=25 && m->anim.animation!=31) : (finished_clip!=25 && finished_clip!=31))) {rc=-4;goto tail;}
            int finished=finished_clip!=65535;
            uint32_t frame=(uint32_t)(int32_t)m->anim.frame;
            m->anim.rate=131072;
            if(finished) {
                rc=play(s,21,0,counts,count);if(rc<0)goto tail;
                m->yaw_delta=2048;rc=basis_update(m,basis,context);if(rc<0)goto tail;m->yaw_delta=0;
            }
            rc=jump_request(s,counts,count);if(rc<0)goto tail;
            if(!finished) {
                m->yaw_delta=(int32_t)((frame<<11)/((uint32_t)counts[31]-1u));
                rc=basis_update(m,basis,context);if(rc<0)goto tail;m->yaw_delta=0;
            }
            goto tail;
        }
        if(input->jump_pressed&&holding<0) {rc=jump_request(s,counts,count);goto tail;}
        if(entry==0x10) ROUTE(SMN64_ROUTE_RUN_AFTER_JUMP);
        rc=holding>=0?smn64_locomotion_grounded_ai_carrying(m,input->camera_yaw,counts,count,basis,context,finished_clip,(uint32_t)holding):
            smn64_locomotion_grounded_ai_prepared(m,input->camera_yaw,counts,count,basis,context,finished_clip);
        if(rc==-2) {rc=play(s,random_range(s,100)<50?293:294,0,counts,count);}
        turn_done=1;
    } else if(entry==0x40) {
        m->idle_ticks=0;
        if(m->anim.finished && (m->anim.animation==210 || m->anim.animation==223)) {
            uint16_t next=m->anim.animation==210?211:(random_range(s,2)?231:224);
            rc=play(s,next,0,counts,count);s->jump_base_timer=7*65536;
            s->launch_velocity=-245760;s->jump_hold_timer=21*32768;m->state=2;
        }
    } else if(entry==2) {
        m->idle_ticks=0;
        ROUTE(SMN64_ROUTE_RISING);
        /* First slice uses original Normal difficulty: release clears extension. */
        if(!input->jump_held)s->jump_hold_timer=0;
        if(s->jump_base_timer<=0 && s->jump_hold_timer<=0)m->state=4;
    } else if(entry==4) {
        m->idle_ticks=0;
        if(contact->grounded) {ROUTE(SMN64_ROUTE_FALLING_LAND);rc=landing(s,counts,count);goto tail;}
        ROUTE(SMN64_ROUTE_FALLING);
        int crossed=((uint32_t)m->velocity_y^(uint32_t)s->previous_velocity_y)&0x80000000u;
        if(crossed) {
            s->falling_origin_y=s->position[1];
            uint16_t a=m->anim.animation;
            if(a==224)rc=play(s,225,0,counts,count);
            else if(a==231)rc=play(s,232,0,counts,count);
            else if(a!=232 && a!=216)rc=play(s,212,0,counts,count);
        }
    } else if(entry==8) {
        m->idle_ticks=0;
        {int lost=lost_ground(s,counts,count,&holding,route,context);if(lost){rc=lost;goto tail;}}
        /* Source landing can be interrupted by aligned movement or a jump. */
        int active=m->analog_x || m->analog_y;
        uint16_t target=(uint16_t)((input->camera_yaw+m->input_angle)&4095);
        uint16_t diff=(uint16_t)((target-smn64_locomotion_heading(m))&4095);
        int moved=0;
        if(active) {
            uint16_t a=m->anim.animation;
            if(diff>=1537 && diff<=2559 && (a==0 || a==11 || a==12 || a==13)) {
                m->state=0x400000;m->turn_ticks=0;rc=play(s,31,0,counts,count);moved=1;
            } else if(diff<128 || diff>3968) {
                m->state=0x10;rc=play(s,1,0,counts,count);moved=1;
            } else {
                uint16_t td=(uint16_t)((m->turn_target-target)&4095);
                if(!m->turn_ticks || (td>=64 && td<=4032))smn64_locomotion_turn(m,target,1);
            }
        }
        if(!moved && !input->jump_pressed)motion_input_enabled=s->jump_variant!=0;
        if(moved) { /* Original CheckForwards returned before the jump check. */ }
        else if(input->jump_pressed) {rc=jump_request(s,counts,count);}
        else {
            ROUTE(SMN64_ROUTE_LANDING_AFTER_JUMP);
            if(finished_clip!=65535) {
            if(s->jump_variant && input->jump_held) {
                s->launch_velocity=-245760;m->state=0x40;
                s->jump_hold_timer=signed32((uint32_t)s->jump_base_timer+sar((uint32_t)s->jump_base_timer,1));
                if(s->collision&1u)m->run_ramp=0;
                rc=play(s,223,0,counts,count);
            } else if(s->jump_variant) m->state=0x10;
            else rc=stand(s,counts,count);
            }
        }
    } else rc=-3;

#undef ROUTE
tail:
    if(rc<0) {out->awaiting_contact=0;out->suspended=rc;return rc;}
    if(!turn_done) {
        int active=m->analog_x || m->analog_y;
        if(m->state==0x10 || ((m->state&0x4eu) && active)) {
            uint16_t target=(uint16_t)((input->camera_yaw+m->input_angle)&4095);
            if(m->state&6u)smn64_locomotion_turn_air(m,target,0,1);
            else smn64_locomotion_turn(m,target,0);
        }
        else if((m->state&0x4eu) || (m->state==1 && !active))m->turn_ticks=0;
        smn64_locomotion_turn_advance(m);
    }
    if(holding>=0)smn64_locomotion_velocity_carrying(m,(uint32_t)holding);else smn64_locomotion_target_velocity(m);
    if(m->state==2 || m->state==4 || (m->state==8 && motion_input_enabled)) {
        int active=m->analog_x || m->analog_y;
        if(active || !(m->state&0x40040004u)) {
            uint16_t h=smn64_locomotion_heading(m);
            if((m->state&6u) && !s->jump_variant)
                h=(uint16_t)((input->camera_yaw+m->input_angle)&4095);
            smn64_locomotion_velocity_at(m,h);
        }
    }
    smn64_jump_tail(&m->velocity_y,s->launch_velocity,&s->jump_base_timer,
                   &s->jump_hold_timer,s->collision,input->elapsed_ticks);
    *out=*s;return 1;
}

int smn64_player_finish_with_hooks(SmN64Player *s,const SmN64Input *input,const SmN64Contact *contact,const uint16_t *counts,size_t n,SmN64PlayerRoute route,SmN64PlayerBasis basis,void *context){return finish_with_hooks(s,input,contact,counts,n,route,basis,context,-1);}
int smn64_player_finish_carrying(SmN64Player *s,const SmN64Input *input,const SmN64Contact *contact,const uint16_t *counts,size_t n,SmN64PlayerRoute route,SmN64PlayerBasis basis,void *context,uint32_t flags){
    if(!s||(s->motion.state!=1&&s->motion.state!=0x10))return -1;
    return finish_with_hooks(s,input,contact,counts,n,route,basis,context,(int)(flags&8u));
}

int smn64_player_finish(SmN64Player *s,const SmN64Input *input,const SmN64Contact *contact,
                       const uint16_t *counts,size_t count) {
    return smn64_player_finish_routed(s,input,contact,counts,count,NULL,NULL);
}

int smn64_player_finish_routed(SmN64Player *s,const SmN64Input *input,const SmN64Contact *contact,
                       const uint16_t *counts,size_t count,SmN64PlayerRoute route,void *context) {
    return smn64_player_finish_with_hooks(s,input,contact,counts,count,route,NULL,context);
}
