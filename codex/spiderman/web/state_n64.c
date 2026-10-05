#include "state_n64.h"
#include "../climbing/clearance_n64.h"
#include "../movement/locomotion_n64.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int32_t si(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static int16_t s16(int32_t v){uint32_t x=(uint32_t)v&65535;return x<=32767?(int16_t)x:(int16_t)(-1-(int32_t)(65535-x));}
static int32_t add(int32_t a,int32_t b){return si((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return si((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return si((uint32_t)a*(uint32_t)b);}
static int32_t sar(int32_t x,unsigned n){uint32_t u=(uint32_t)x;return si((u>>n)|((u&0x80000000u)?(UINT32_MAX<<(32-n)):0u));}
static int32_t abs32(int32_t x){return x<0?sub(0,x):x;}
static void copy(int32_t d[3],const int32_t s[3]){memcpy(d,s,3*sizeof(*d));}
static void zero(int32_t v[3]){memset(v,0,3*sizeof(*v));}
static float f32(float x){volatile float r=x;return r;}
static void norm(int32_t v[3]){
    float x=f32((float)v[0]*0x1p-12f),y=f32((float)v[1]*0x1p-12f),z=f32((float)v[2]*0x1p-12f);
    float len=sqrtf(f32(f32(f32(x*x)+f32(y*y))+f32(z*z)));
    if(len==0){zero(v);return;}
    float inv=f32(1.0f/len);v[0]=(int32_t)f32(f32(x*inv)*4096.0f);v[1]=(int32_t)f32(f32(y*inv)*4096.0f);v[2]=(int32_t)f32(f32(z*inv)*4096.0f);
}
static int32_t distance(const int32_t a[3],const int32_t b[3]){
    uint32_t square=0;for(int i=0;i<3;i++){int32_t v=sar(sub(a[i],b[i]),12);square+=(uint32_t)v*(uint32_t)v;}
    return (int32_t)sqrtf((float)square);
}
int smn64_web_zip_velocity(const int32_t p[3],const int32_t t[3],int32_t v[3]){
    int32_t x=sar(sub(t[0],p[0]),12),y=sar(sub(t[1],p[1]),12),z=sar(sub(t[2],p[2]),12);
    int32_t yaw,pitch;
    if(z){
        /* Source signed DIV result, then original single-argument atan. */
        int32_t numerator=si((uint32_t)x<<12),denominator=z>0?sub(0,z):z;
        if(numerator==INT32_MIN&&denominator==-1)return -2;
        int32_t ratio=numerator/denominator;
        yaw=smn64_locomotion_atan(ratio,4096);if(z>0)yaw=sub(2048,yaw);
    }else yaw=x>0?-1024:1024;
    uint32_t sq=(uint32_t)x*(uint32_t)x+(uint32_t)z*(uint32_t)z;
    int32_t horizontal=(int32_t)sqrtf((float)sq);
    if(horizontal){
        uint32_t num=(uint32_t)(y>0?y:sub(0,y))<<12;
        pitch=smn64_locomotion_atan(si(num/(uint32_t)horizontal),4096);if(y<=0)pitch=sub(0,pitch);
    }else pitch=y>0?1024:-1024;
    yaw&=4095;pitch&=4095;
    int32_t c=smn64_locomotion_cos(pitch);
    v[0]=sub(0,mul(sar(mul(240,c),12),smn64_locomotion_sin(yaw)));
    v[1]=mul(240,smn64_locomotion_sin(pitch));
    v[2]=sub(0,mul(sar(mul(240,c),12),smn64_locomotion_cos(yaw)));
    return 1;
}
int smn64_web_zip_reached(const int32_t p[3],const int32_t t[3],const int32_t n[3]){
    uint32_t d=0;for(int i=0;i<3;i++)d+=(uint32_t)mul(sar(sub(p[i],t[i]),12),n[i]);
    return si(d)<0 || distance(p,t)<128;
}
typedef struct ClearanceContext {SmN64WebRay ray;void *context;} ClearanceContext;
static int clearance_trace(void *context,const SmN64ClimbQuery *query,SmN64ClimbHit *hit){
    ClearanceContext *c=context;SmN64WebLine line;memset(&line,0,sizeof(line));
    if(c->ray(c->context,SMN64_WEB_CLEARANCE,query->start,query->end,&line)!=1)return -3;
    hit->present=line.hit!=0;memcpy(hit->normal,line.normal,sizeof(hit->normal));return 1;
}
int smn64_web_clearance(SmN64WebPlayer *s,int32_t dist,SmN64WebRay ray,void *ctx){
    if(!s||!ray)return -3;
    SmN64ClimbBasis basis;memset(&basis,0,sizeof(basis));copy(basis.forward,s->forward);copy(basis.right,s->right);
    ClearanceContext c={ray,ctx};return smn64_climb_clearance(s->position,&basis,dist,clearance_trace,&c);
}
static void emit(SmN64WebEvents *e,SmN64WebEventKind k,int32_t v,int32_t x){if(e->count<16)e->events[e->count++]=(SmN64WebEvent){k,v,x};}
static void play(SmN64WebPlayer *p,uint16_t clip,int from,const uint16_t *c){smn64_anim_run(&p->anim,clip,c[clip],from,-1);}
static int orient(SmN64WebRuntime *s,const int32_t *f,const SmN64WebServices *q,SmN64WebEvents *e){
    SmN64WebPlayer *p=&s->player;memcpy(s->basis.normal,p->normal,sizeof(p->normal));
    copy(s->basis.forward,p->forward);copy(s->basis.right,p->right);copy(s->basis.outward,p->outward);
    int regular=smn64_climb_basis(&s->basis,f);
    copy(p->forward,s->basis.forward);copy(p->right,s->basis.right);copy(p->outward,s->basis.outward);
    /* Original9D258 writes its exact fallback matrix (retaining the old
     * inverse), then invokes97DE4. Do not normalize a degenerate direction or
     * fabricate a continuation when the enclosing source stop is unavailable. */
    if(!regular && (!q->action||q->action(q->context,SMN64_WEB_ACTION_STOP,s,e)!=1))return -5;
    return 1;
}
static int action(const SmN64WebServices *q,SmN64WebStateAction a,SmN64WebRuntime *s,SmN64WebEvents *e){return q->action?q->action(q->context,a,s,e):-4;}
static void consume(SmN64WebPlayer *p,SmN64WebEvents *e){
    SmN64WebResourceEvent r;smn64_web_consume(&p->resource,128,p->random_state,&r);
    if(r.sound)emit(e,SMN64_WEB_REFILL_SOUND,r.sound,0);
    if(r.voice_group)emit(e,SMN64_WEB_EMPTY_VOICE,r.voice_group,r.voice_variant);
}
static int unavailable_lifecycle(void *ctx,SmN64WebEventKind kind,SmN64WebPlayer *p,const struct SmN64Swinger *s){(void)ctx;(void)kind;(void)p;(void)s;return 0;}
static int lifetime(const SmN64WebServices *q,SmN64WebEventKind kind,SmN64WebRuntime *s){return q->lifecycle&&q->lifecycle(q->context,kind,&s->player,&s->swinger)==1?1:-6;}
static int destroy_swing(SmN64WebRuntime *s,const SmN64WebServices *q,SmN64WebEvents *e){SmN64WebPlayer *p=&s->player;if(p->swinger_present){if(lifetime(q,SMN64_WEB_DETACH_STRAND,s)<0||lifetime(q,SMN64_WEB_DELETE_SWINGER,s)<0)return -6;emit(e,SMN64_WEB_DETACH_STRAND,0,0);emit(e,SMN64_WEB_DELETE_SWINGER,0,0);p->swinger_present=0;}return 1;}
static int new_swing(SmN64WebRuntime *s,const int32_t a[3],const SmN64WebServices *q,SmN64WebEvents *e){
    SmN64WebPlayer *p=&s->player;s->swing_length=distance(p->position,a);
    smn64_swinger_basis(p->position,a,p->target_normal,s->swing_basis,&s->swing_vertical_abs);
    smn64_swinger_init(&s->swinger,a,s->swing_length,s->swing_basis,p->target_normal,s->now);
    p->swinger_present=1;if(lifetime(q,SMN64_WEB_CREATE_SWINGER,s)<0)return -6;emit(e,SMN64_WEB_CREATE_SWINGER,s->swing_length,p->resource.web_type);
    emit(e,SMN64_WEB_SOUND_GLOBAL,(int32_t)smn64_web_random(p->random_state,3)+21,10000);return 1;
}
static int zip_step(SmN64WebRuntime *s,const SmN64WebInput *in,const SmN64WebServices *q,const uint16_t *c,SmN64WebEvents *e){
    SmN64WebPlayer *p=&s->player;p->idle_ticks=0;uint16_t clip=p->anim.animation;
    if((clip==250||clip==260)&&p->anim.finished){
        play(p,270,0,c);p->zip_graphic=1;p->web_mode=8;if(lifetime(q,SMN64_WEB_CREATE_ZIP,s)<0)return -6;emit(e,SMN64_WEB_CREATE_ZIP,p->resource.web_type,0);
        p->resource.allow_empty=1;consume(p,e);if(lifetime(q,SMN64_WEB_FIRE_ZIP,s)<0)return -6;p->resource.allow_empty=0;emit(e,SMN64_WEB_FIRE_ZIP,128,1);emit(e,SMN64_WEB_SOUND_POSITION,21,0);return 1;
    }
    if((clip==270&&p->anim.frame>=13)||clip==271){
        if(clip==270){p->adhered=0;if(p->zip_graphic){if(lifetime(q,SMN64_WEB_DELETE_ZIP,s)<0)return -6;emit(e,SMN64_WEB_DELETE_ZIP,0,0);p->zip_graphic=0;}}
        if((s->damage_flags&0x40000u)&&(uint32_t)(s->now-s->damage_tick)<6u){
            play(p,175,0,c);p->state=0x800000;p->launch_flag=0;zero(p->velocity);s->field664=0;return 1;
        }
        if(smn64_web_zip_reached(p->position,p->target,p->target_normal)){
            s->field_d20=0;emit(e,SMN64_WEB_CAMERA_TARGET,1,0);
            for(int i=0;i<3;i++)p->position[i]=add(p->target[i],mul(p->target_normal[i],p->body_offset));
            zero(p->velocity);
            const int32_t *f=NULL;int32_t temp[3];
            if(p->target_normal[1]<-2600||p->target_normal[1]>3400){
                if(abs32(p->forward[1])<2048)copy(s->retained_forward,p->forward);
                else{
                    for(int i=0;i<3;i++)temp[i]=sar(sub(p->zip_origin[i],p->target[i]),12);
                    norm(temp);
                    if(temp[1]<2048)copy(s->retained_forward,temp);else{s->retained_forward[0]=s->retained_forward[1]=0;s->retained_forward[2]=4096;}
                }
                f=s->retained_forward;
                for(int i=0;i<3;i++)p->normal[i]=s16(p->target_normal[i]);
                int rc=orient(s,f,q,e);if(rc<0)return rc;p->turn_ticks=0;
            }else{
                for(int i=0;i<3;i++)temp[i]=sar(sub(p->zip_origin[i],p->target[i]),12);
                    norm(temp);
                int32_t dot=0;for(int i=0;i<3;i++)dot=add(dot,sar(mul(temp[i],p->target_normal[i]),12));
                if(dot<2048){copy(s->retained_forward,temp);f=s->retained_forward;}
                else if(abs32(p->normal[1])>2048){for(int i=0;i<3;i++)temp[i]=-(int32_t)p->normal[i];f=temp;}
                for(int i=0;i<3;i++)p->normal[i]=s16(p->target_normal[i]);
                int rc=orient(s,f,q,e);if(rc<0)return rc;
            }
            p->adhered=1;play(p,272,0,c);emit(e,SMN64_WEB_SOUND_GLOBAL,9,10000);
            return smn64_web_clearance(p,32,q->ray,q->context);
        }
        if(in->jump_pressed||(in->kid_mode&&(in->web_pressed||in->punch_pressed||in->kick_pressed))){
            p->state=4;p->launch_flag=p->lock=1;zero(p->velocity);p->normal[0]=p->normal[2]=0;p->normal[1]=-4096;copy(s->retained_forward,p->forward);return orient(s,s->retained_forward,q,e);
        }
        if(smn64_web_zip_velocity(p->position,p->target,p->velocity)<0)return -2;
        if(p->velocity[1]<0&&abs32(p->velocity[0])<abs32(p->velocity[1])&&abs32(p->velocity[2])<abs32(p->velocity[1])){
            int rc=smn64_web_try_swing(p,in,q->ray,q->context,c,300,e);if(rc<0)return rc;if(rc)zero(p->velocity);
        }
        return 1;
    }
    if(clip==272){
        if(!p->wall_orientation&&!p->ceiling_orientation)p->adhered=0;
        int rc=action(q,SMN64_WEB_ACTION_JUMP,s,e);if(rc<0)return rc;if(rc)return 1;
        if(p->anim.finished){
            if(!p->wall_orientation&&!p->ceiling_orientation)play(p,20,0,c);
            p->lock=1;if(p->anim.animation!=20)play(p,p->adhered?19:0,0,c);p->state=1;
        }
    }
    return 1;
}
static int swing_launch(SmN64WebRuntime *s,const SmN64WebServices *q,const uint16_t *c,SmN64WebEvents *e){
    (void)c;SmN64WebPlayer *p=&s->player;p->idle_ticks=0;p->airborne_owner=1;if(p->anim.frame<13)return 1;
    s->launch_velocity=add(-245760,s->platform_present?s->platform_velocity_y:0);p->state=0x200;p->substate=0;
    emit(e,SMN64_WEB_SOUND_POSITION,9,0);s->base_timer=7*65536;s->hold_timer=21*32768;p->normal[0]=p->normal[2]=0;p->normal[1]=-4096;
    if(p->swing_target[0]||p->swing_target[1]||p->swing_target[2]){
        int32_t f[3]={sar(sub(p->position[0],p->swing_target[0]),12),0,sar(sub(p->position[2],p->swing_target[2]),12)};norm(f);int rc=orient(s,f,q,e);if(rc<0)return rc;p->wall_orientation=0;
    }else return orient(s,NULL,q,e);
    return 1;
}
static int swing_rise(SmN64WebRuntime *s,const SmN64WebServices *q,const uint16_t *c,SmN64WebEvents *e){
    SmN64WebPlayer *p=&s->player;
    if(!p->substate){emit(e,SMN64_WEB_CAMERA,2,0);emit(e,SMN64_WEB_CAMERA_TURN,16,0);p->substate=add(p->substate,1);}
    if((s->damage_flags&0x200u)&&(uint32_t)(s->now-s->damage_tick)<6u){
        int32_t floor;if(!q->floor||q->floor(q->context,p->position,0,4096,1,&floor)!=1)return -4;
        if(floor!=-1){play(p,175,0,c);p->velocity[0]=p->velocity[2]=0;p->state=0x800000;s->field664=0;}
        else{play(p,216,0,c);p->state=4;}p->launch_flag=0;
    }
    int rc=action(q,SMN64_WEB_ACTION_CEILING,s,e);if(rc<0)return rc;if(rc)return 1;
    if(p->velocity[1]<0){s->analog_x=-127;s->analog_y=0;s->run_ramp=16;return 1;}
    p->state=0x400;play(p,275,0,c);emit(e,SMN64_WEB_SOUND_POSITION,25,0);consume(p,e);return new_swing(s,p->anchor,q,e);
}
static int swing_active(SmN64WebRuntime *s,const SmN64WebInput *in,const SmN64WebServices *q,const uint16_t *c,SmN64WebEvents *e){
    SmN64WebPlayer *p=&s->player;p->idle_ticks=0;
    int rc=action(q,SMN64_WEB_ACTION_AIR_ATTACK,s,e);if(rc<0)return rc;if(rc)return 1;
    rc=smn64_web_try_zip_b_visual(p,in,q->ray,q->context,c,300,e,q->lifecycle?q->lifecycle:unavailable_lifecycle);if(rc<0)return rc;if(rc)return 1;
    int done=0;
    if(p->swinger_present){
        if(smn64_swinger_complete(&s->swinger)){
            if(p->second_web){
                play(p,280,0,c);for(int i=0;i<3;i++)p->position[i]=sub(p->position[i],mul(p->forward[i],192));
                consume(p,e);if(destroy_swing(s,q,e)<0)return -6;if(new_swing(s,p->second_anchor,q,e)<0)return -6;
                p->anim.frame=smn64_swinger_animation_frame(&s->swinger,(int32_t)c[p->anim.animation]-1);p->second_web=0;emit(e,SMN64_WEB_SOUND_POSITION,25,0);
            }else done=1;
        }else{p->anim.rate=0;p->anim.frame=smn64_swinger_animation_frame(&s->swinger,(int32_t)c[p->anim.animation]-1);}
    }
    if(!(s->collision&3u)&&done){
        int32_t from[3],to[3];if(!q->marker||q->marker(q->context,s,2,from)!=1)return -4;
        for(int i=0;i<3;i++)to[i]=sub(from[i],mul(p->forward[i],256));
        SmN64WebLine h;memset(&h,0,sizeof(h));if(q->ray(q->context,SMN64_WEB_SWING_EXIT,from,to,&h)!=1)return -3;
        if(h.hit){memcpy(s->side.normal,h.normal,sizeof(h.normal));s->collision|=1;for(int i=0;i<3;i++)p->position[i]=add(h.position[i],mul(h.normal[i],p->body_offset));}
    }
    if(s->collision&3u){
        zero(p->velocity);emit(e,SMN64_WEB_SOUND_POSITION,9,0);s->field_d20=0;
        if(s->side.normal[1]<-2600){
            p->adhered=0;if(destroy_swing(s,q,e)<0)return -6;memcpy(p->normal,s->side.normal,sizeof(p->normal));p->airborne_owner=0;p->lock=1;play(p,213,0,c);p->state=1;emit(e,SMN64_WEB_CAMERA,-1,0);return 1;
        }
        done=1; /* original DD0 branch delay slot, even when surface is NULL */
        if(s->side.surface){
            if(s->side.normal[1]>=3401&&!(s->side.surface_flags&4)){
                p->adhered=1;if(destroy_swing(s,q,e)<0)return -6;memcpy(p->normal,s->side.normal,sizeof(p->normal));copy(s->retained_forward,p->forward);rc=orient(s,s->retained_forward,q,e);if(rc<0)return rc;p->turn_ticks=0;
            }else if(s->side.normal[1]<3401){
                p->adhered=1;if(destroy_swing(s,q,e)<0)return -6;memcpy(p->normal,s->side.normal,sizeof(p->normal));int32_t f[3]={0,4096,0};rc=orient(s,f,q,e);if(rc<0)return rc;
            }else goto interrupt;
            rc=smn64_web_clearance(p,32,q->ray,q->context);if(rc<0)return rc;
            p->airborne_owner=0;p->lock=1;emit(e,SMN64_WEB_CAMERA,-1,0);play(p,281,0,c);p->state=1;return 1;
        }
    }
interrupt:
    if((s->damage_flags&0x400u)&&(uint32_t)(s->now-s->damage_tick)<6u){play(p,216,0,c);p->state=4;p->launch_flag=0;done=1;}
    if(in->jump_pressed)p->kid_jump_gate=0;
    int jump_exit=in->jump_pressed&&(!in->kid_mode||p->resource.player_2cc);
    if(!done&&!jump_exit&&!(in->kid_mode&&(in->web_pressed||in->punch_pressed||in->kick_pressed)))return 1;
    p->velocity[0]=sar(p->velocity[0],1);p->velocity[2]=sar(p->velocity[2],1);p->anim.rate=65536;
    if(p->state!=4){p->state=4;p->launch_flag=1;}
    emit(e,SMN64_WEB_CAMERA,-1,0);p->airborne_owner=0;p->lock=1;if(destroy_swing(s,q,e)<0)return -6;return 1;
}
int smn64_web_state_step(SmN64WebRuntime *s,const SmN64WebInput *in,const SmN64WebServices *q,const uint16_t *c,size_t count,SmN64WebEvents *events){
    if(!s||!in||!q||!q->ray||!c||count<300||!events||events->count>2)return -1;
    const unsigned clips[]={0,19,20,175,213,216,250,260,270,271,272,273,275,280,281,282};
    for(unsigned i=0;i<sizeof(clips)/sizeof(*clips);i++)if(!c[clips[i]])return -1;
    if(s->player.anim.animation>=count||!c[s->player.anim.animation])return -1;
    SmN64WebRuntime work=*s;SmN64WebEvents e=*events;int rc;
    switch(work.player.state){
        case 0x40000:rc=zip_step(&work,in,q,c,&e);break;
        case 0x100:rc=swing_launch(&work,q,c,&e);break;
        case 0x200:rc=swing_rise(&work,q,c,&e);break;
        case 0x400:rc=swing_active(&work,in,q,c,&e);break;
        default:return -1;
    }
    if(rc<0)return rc;
    *s=work;*events=e;return 1;
}
