#include "web_action_n64.h"
#include "../movement/locomotion_n64.h"
#include <math.h>
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t mul(int32_t a,int32_t b){return s32((uint32_t)a*(uint32_t)b);}
static int32_t sar(int32_t v,unsigned n){return s32(((uint32_t)v>>n)|(v<0?(UINT32_MAX<<(32-n)):0));}
static int run(SmN64WebAction *s,uint16_t clip,const uint16_t *counts,size_t n){if(!counts||clip>=n||!counts[clip]||counts[clip]>INT16_MAX)return -1;smn64_anim_run(&s->ability.anim,clip,counts[clip],0,-1);return 1;}
int smn64_yank_curve(const int32_t player[3],const int32_t target[3],SmN64YankCurve *out){
    int32_t d[3],direction[3],distance,shrink,y;uint32_t sq=0;unsigned i,j;
    if(!player||!target||!out)return -1;
    memset(out,0,sizeof(*out));y=sub(target[1],player[1]);if(y<0)y=s32(0u-(uint32_t)y);if(y>524288)return 0;
    for(j=0;j<3;j++){d[j]=sar(sub(target[j],player[j]),12);sq+=(uint32_t)d[j]*(uint32_t)d[j];}
    distance=(int32_t)sqrtf((float)sq);d[1]=0;smn64_combat_normalize(d,direction);shrink=sub(distance,256)/7;
    for(i=0;i<8;i++){
        int32_t radial=sar(mul(smn64_locomotion_cos((int32_t)i*256),distance),12);
        out->points[i][0]=add(player[0],mul(direction[0],radial));out->points[i][2]=add(player[2],mul(direction[2],radial));
        if(i<4)distance=sub(distance,shrink);
        out->points[i][1]=sub(player[1],s32((uint32_t)smn64_locomotion_sin((int32_t)i*256)<<8));
    }
    out->present=1;return 1;
}
static int stop(SmN64WebAction *s,const SmN64WebActionHost *h){return h->stop&&h->stop(h->context,s)==1?1:-2;}
static int jump(SmN64WebAction *s,const SmN64WebActionHost *h){return h->jump?h->jump(h->context,s):-2;}
static int release(SmN64WebAction *s,const SmN64WebActionHost *h,int immediate){
    if(s->graphic){if(!h->release_graphic||h->release_graphic(h->context,s->graphic,(uint8_t)immediate)!=1)return -2;s->graphic=0;}return 1;
}
static int fire(SmN64WebAction *s,const SmN64WebActionHost *h,int32_t amount,int first,uint32_t *flags){
    static const int16_t zero[3]={0,0,0};
    return h->fire&&h->fire(h->context,s,(uint8_t)(s->ability.anim.animation==148||!s->ability.aim_snapshot),amount,(uint8_t)(first&&s->ability.aim_snapshot),first&&s->ability.aim_snapshot?s->target_normal:zero,flags)==1?1:-2;
}
static int early(SmN64WebAction *s,const SmN64WebButtons *buttons,uint32_t tick,SmN64WebResource *r,uint32_t rng[3],const uint16_t *c,size_t n,const SmN64WebActionHost *h,SmN64WebAbilityEvent *e,SmN64CombatAdmission admit,void *admit_context){
    SmN64WebAbility *a=&s->ability;int adhered=a->anim.animation==260;int rc;
    if(a->axis_1123>0&&!(a->blocked_directions&1)){
        if(admit){rc=admit(admit_context,SMN64_COMMAND_YANK);if(rc!=1)return rc==0?2:-2;}
        a->attack_mode=2;a->fired=0;a->state=0x8000;a->yank_variant=a->axis_1124>0?2:a->axis_1124<0?1:0;a->actor_flags&=0xfe;return run(s,adhered?262:252,c,n);
    }
    if(a->axis_1123<0&&!(a->blocked_directions&2)){
        if(admit){rc=admit(admit_context,SMN64_COMMAND_IMPACT);if(rc!=1)return rc==0?2:-2;}
        a->attack_mode=4;a->fired=0;a->state=0x10000;a->actor_flags&=0xfe;return run(s,adhered?265:255,c,n);
    }
    if(!adhered&&a->axis_1124<0&&!(a->blocked_directions&8)&&tick-a->last_glove_tick>=31&&!a->surface_mode){
        if(admit){rc=admit(admit_context,SMN64_COMMAND_GLOVES);if(rc!=1)return rc==0?2:-2;}
        rc=smn64_web_consume(r,1024,rng,&e->resource);if(rc<0)return rc;if(!rc)return 2;
        a->state=0x800000;return run(s,285,c,n);
    }
    if(!adhered&&a->axis_1124>0&&!(a->blocked_directions&4)){
        if(a->surface_mode)return 0;
        if(admit){rc=admit(admit_context,SMN64_COMMAND_DOME);if(rc!=1)return rc==0?2:-2;}
        rc=smn64_web_consume(r,3072,rng,&e->resource);if(rc<0)return rc;if(!rc)return 0;
        if(rc){a->state=0x20000000;a->dome_tick=tick;if(run(s,283,c,n)<0)return -1;if(!h->dome||h->dome(h->context,s,(uint32_t)r->web_type)!=1)return -2;return 1;}
    }
    if(!adhered&&!a->aiming&&(buttons->punch||buttons->kick)){
        if(admit){rc=admit(admit_context,SMN64_COMMAND_GRAB);if(rc!=1)return rc==0?2:-2;}
        if(!h->grab||h->grab(h->context,s)!=1)return -2;
        a->state=0x2000000;return run(s,120,c,n);
    }
    return 0;
}
int smn64_web_action_step_admitted(SmN64WebAction *s,const SmN64WebButtons *buttons,uint32_t tick,
    SmN64WebResource *r,uint32_t rng[3],const uint16_t *c,size_t n,const SmN64WebActionHost *h,SmN64WebAbilityEvent *e,SmN64CombatAdmission admit,void *admit_context){
    SmN64WebAbility *a;uint16_t clip;int16_t frame;int rc;uint32_t flags=0;unsigned i;
    if(!s||!buttons||!r||!rng||!h||!e)return -1;
    memset(e,0,sizeof(*e));a=&s->ability;clip=a->anim.animation;frame=a->anim.frame;
    if(a->state==0x4000){
        if(clip==250||clip==260){
            if(frame<5){rc=early(s,buttons,tick,r,rng,c,n,h,e,admit,admit_context);if(rc)return rc<0?rc:1;}
            if(a->anim.finished){if(release(s,h,0)<0)return -2;
                return run(s,clip==260?261:251,c,n);}
            if(frame<5)return 1;
            if(!s->graphic){
                if(a->fired)return 1;
                a->fired=1;if(!h->create_graphic||h->create_graphic(h->context,0,(uint32_t)r->web_type,&s->graphic)!=1||!s->graphic)return -2;
                if(fire(s,h,256,1,&flags)<0)return -2;
                if(flags&1)return release(s,h,1);
                if(!(flags&2))return release(s,h,0);
            }else if(buttons->web){
                uint32_t target;if(!h->graphic_target||h->graphic_target(h->context,s->graphic,&target)!=1)return -2;
                if(target!=s->target){if(release(s,h,0)<0)return -2;}
                if(s->graphic){
                    if(fire(s,h,128,0,&flags)<0)return -2;
                    if(flags&1){if(release(s,h,1)<0)return -2;}
                    else if(!(flags&2)){if(release(s,h,0)<0)return -2;}
                    if(s->graphic)a->anim.frame=5;
                }
            }
            return 1;
        }
        if(clip==251||clip==261){rc=jump(s,h);if(rc<0)return -2;if(rc)return 1;if(a->anim.finished){a->moving_latch=0;return stop(s,h);}}
        return 1;
    }
    if(a->state==0x10000){
        if(clip==139){
            if(a->anim.finished)return stop(s,h);
            if(frame<3)return 1;
            if(!a->fired){a->fired=1;if(fire(s,h,900,0,&flags)<0)return -2;}
            else {rc=jump(s,h);if(rc<0)return -2;if(rc)return 1;}
            if(frame<6){if(!h->impact_sparks||h->impact_sparks(h->context,4)!=1)return -2;}
            return 1;
        }
        if(clip==255||clip==265){
            if(frame<5)return 1;
            if(!a->fired){a->fired=1;if(fire(s,h,900,0,&flags)<0)return -2;}
            if(frame<12){if(!h->impact_sparks||h->impact_sparks(h->context,2)!=1)return -2;}
            if(a->anim.finished)return run(s,clip==265?266:256,c,n);
            return 1;
        }
        if(clip==256||clip==266){rc=jump(s,h);if(rc<0)return -2;if(rc)return 1;if(a->anim.finished)return stop(s,h);}
        return 1;
    }
    if(a->state==0x8000){
        if(clip==148){
            if(!a->fired){
                if(frame<5||s->graphic)return 1;
                a->fired=1;s->hold_timer=40;if(!h->create_graphic||h->create_graphic(h->context,0,(uint32_t)r->web_type,&s->graphic)!=1||!s->graphic)return -2;
                if(fire(s,h,600,0,&flags)<0)return -2;
                if(flags&1)return release(s,h,1);
                if((flags&8)||!s->target)return release(s,h,0);
                a->state=0x20000;s->phase=0;return 1;
            }
            if(frame>=7){if(!c||154>=n||!c[154])return -1;smn64_anim_run(&a->anim,154,c[154],frame,-1);}return 1;
        }
        if(clip==252||clip==262){
            int threshold=clip==262?9:5;
            if(!a->fired&&frame>=threshold&&!s->graphic){
                s->hold_timer=clip==262?64:40;a->fired=1;
                if(!h->create_graphic||h->create_graphic(h->context,1,(uint32_t)r->web_type,&s->graphic)!=1||!s->graphic)return -2;
                if(fire(s,h,600,1,&flags)<0)return -2;
                if(flags&1){if(release(s,h,1)<0)return -2;}
            }
            if(a->anim.finished){
                if(s->target&&s->target_type==0x197)s->target=0;
                s->hold_timer=(uint16_t)(s->hold_timer-(uint16_t)a->anim.elapsed_ticks);
                if(!s->graphic)return run(s,clip==262?263:253,c,n);
                if(s->target){a->state=0x20000;s->phase=0;return run(s,clip==262?264:254,c,n);}
                if(release(s,h,0)<0)return -2;
                return run(s,clip==262?263:253,c,n);
            }
            return 1;
        }
        if(clip==253||clip==263||clip==154){rc=jump(s,h);if(rc<0)return -2;if(rc)return 1;if(a->anim.finished)return stop(s,h);}
        return 1;
    }
    if(a->state==0x20000){
        if(s->phase){rc=jump(s,h);if(rc<0)return -2;if(rc)return 1;}
        if(s->graphic){
            int pull=(clip==148&&frame>=14)||((clip==257||clip==258||clip==259||clip==267||clip==268||clip==269)&&frame>=9);
            if(pull){int32_t velocity[3],target[3];SmN64YankCurve curve;memset(&curve,0,sizeof(curve));
                if(clip==148){uint32_t attached=0;
                    if(!h->graphic_target||h->graphic_target(h->context,s->graphic,&attached)!=1||!attached||
                       !h->target_position||h->target_position(h->context,attached,target)!=1)return -2;
                    smn64_yank_curve(s->position,target,&curve);
                }
                for(i=0;i<3;i++){
                    if(clip==257||clip==267)velocity[i]=add(add(mul(s->right[i],-24),mul(s->up[i],16)),mul(s->forward[i],24));
                    else if(clip==258||clip==268)velocity[i]=add(add(mul(s->right[i],24),mul(s->up[i],16)),mul(s->forward[i],24));
                    else velocity[i]=add(mul(s->forward[i],32),mul(s->up[i],16));
                }
                if(!h->yank||h->yank(h->context,s->graphic,velocity,&curve)!=1)return -2;
                s->graphic=0;s->phase=1;
            }
        }
        if(a->anim.finished){
            if(clip==148){e->face_target=2;return stop(s,h);} /*forced basis toward-forward negation, thenstop*/
            if(clip==254||clip==264){uint16_t next=a->yank_variant==0?259:a->yank_variant==1?258:257;if(clip==264)next+=10;return run(s,next,c,n);}
            return stop(s,h);
        }
        return 1;
    }
    return 0;
}

int smn64_web_action_step(SmN64WebAction *s,const SmN64WebButtons *buttons,uint32_t tick,
    SmN64WebResource *r,uint32_t rng[3],const uint16_t *counts,size_t n,
    const SmN64WebActionHost *host,SmN64WebAbilityEvent *event){
    return smn64_web_action_step_admitted(s,buttons,tick,r,rng,counts,n,host,event,NULL,NULL);
}
