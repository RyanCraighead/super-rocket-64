#include "fireweb_n64.h"
#include "../movement/locomotion_n64.h"
#include <math.h>
#include <string.h>
static int32_t s32(uint32_t v){return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v;}
static int32_t add(int32_t a,int32_t b){return s32((uint32_t)a+(uint32_t)b);}
static int32_t sub(int32_t a,int32_t b){return s32((uint32_t)a-(uint32_t)b);}
static int32_t sar(int32_t v,unsigned n){return s32(((uint32_t)v>>n)|(v<0?(UINT32_MAX<<(32-n)):0));}
static int quotient(int32_t a,int32_t b,int32_t *q){if(!b||(a==INT32_MIN&&b==-1))return -1;*q=a/b;return 1;}
int32_t smn64_fireweb_angles(const int32_t from[3],const int32_t to[3],int16_t angles[3]){
    int32_t x,y,z,yaw,pitch,ratio;uint32_t distance;
    if(!from||!to||!angles)return -1;
    x=sar(sub(to[0],from[0]),12);y=sar(sub(to[1],from[1]),12);z=sar(sub(to[2],from[2]),12);
    if(z){
        if(quotient(s32((uint32_t)x<<12),z>0?-z:z,&ratio)<0)return -1;
        yaw=smn64_locomotion_atan(ratio,4096);if(z>0)yaw=sub(2048,yaw);
    }else yaw=x>0?-1024:1024;
    distance=(uint32_t)sqrtf((float)((uint32_t)x*(uint32_t)x+(uint32_t)z*(uint32_t)z));
    if(distance){
        uint32_t numerator=(y>0?(uint32_t)y:0u-(uint32_t)y)<<12;
        pitch=smn64_locomotion_atan(s32(numerator/distance),4096);if(y<=0)pitch=sub(0,pitch);
    }else pitch=y>0?1024:-1024;
    angles[0]=(int16_t)((uint32_t)pitch&4095);angles[1]=(int16_t)((uint32_t)yaw&4095);angles[2]=0;return (int32_t)distance;
}
int smn64_fireweb(SmN64FireWeb *s,uint8_t auto_target,int32_t amount,const int32_t explicit_target[3],
    uint8_t attached,const int16_t normal[3],SmN64WebResource *resource,uint32_t rng[3],
    const SmN64FireHost *h,SmN64FireEvent *e){
    SmN64FireActor actor,special;SmN64FireSurface surface;uint32_t special_id=0;int32_t point[3];unsigned i;int rc;
    if(!s||!explicit_target||!normal||!resource||!rng||!h||!h->actor||!h->bone||!h->special_target||!h->trace||!e)return -1;
    memset(e,0,sizeof(*e));memset(&actor,0,sizeof(actor));memset(&special,0,sizeof(special));
    memcpy(e->normal,normal,sizeof(e->normal));
    if(s->target_id){rc=h->actor(h->context,s->target_id,&actor);if(rc<0||!actor.id)return -2;if(actor.type==0x197)special_id=actor.id;}
    if(auto_target){
        attached=0;
        if(actor.id)memcpy(e->target,actor.position,sizeof(e->target));
        else {
            uint32_t id=0;rc=h->special_target(h->context,point,&id);if(rc<0)return -2;s->special_point_id=id;
            if(rc){for(i=0;i<3;i++)e->target[i]=add(point[i],sar(sub(point[i],s->position[i]),4));}
            else {rc=h->bone(h->context,2,e->target);if(rc!=1)return -2;for(i=0;i<3;i++)e->target[i]=sub(e->target[i],s32((uint32_t)s->forward[i]*2048u));}
            memset(&surface,0,sizeof(surface));rc=h->trace(h->context,s->position,e->target,&surface);if(rc<0)return -2;
            if(surface.surface_present&&surface.hit){
                attached=1;memcpy(e->target,surface.position,sizeof(e->target));memcpy(e->normal,surface.normal,sizeof(e->normal));
                if(surface.special_actor){
                    rc=h->actor(h->context,surface.special_actor,&special);if(rc<0||!special.id)return -2;
                    if(special.special_enabled)special_id=special.id;
                }
            }
        }
    }else memcpy(e->target,explicit_target,sizeof(e->target));
    e->surface_attached=attached;
    if(s->graphic_id){
        rc=smn64_web_consume(resource,amount,rng,&e->resource);if(rc<0)return rc;
        if(!rc){e->result_flags=1;return 1;}
        rc=h->bone(h->context,(uint8_t)s->graphic_hand,e->muzzle);if(rc!=1)return -2;
        e->update_graphic=1;e->graphic_id=s->graphic_id;
        e->attach_actor=s->attack_mode!=8&&actor.id&&actor.type!=0x197?actor.id:0;
        if(special_id){e->result_flags=4;e->activate_special=special_id;e->sound=0x15;e->sound_positional=1;return 1;}
        if(actor.id&&s->attack_mode==1){
            if(!(actor.flags_3c8&0x10000)&&actor.health>0){
                e->actor_message=5;e->actor_message_target=actor.id;e->trap_samples=8;e->trap_web_type=(uint32_t)resource->web_type;e->result_flags=2;
                if(!s->loop_sound){e->sound=0x15;e->sound_positional=1;e->loop_sound=0x21;s->loop_sound=1;}
            }
            return 1;
        }
        if(s->attack_mode==2){
            if(actor.id&&!(actor.flags_3c8&0x200)&&actor.health>0){
                e->actor_message=6;e->actor_message_target=actor.id;e->yank_mark=actor.id;e->result_flags=2;e->sound=0x15;e->sound_positional=1;return 1;
            }
            e->rejected_yank_actor=actor.id;
            e->release_graphic=s->graphic_id;s->graphic_id=0;e->sound=0x16;e->result_flags=8;return 1;
        }
        e->sound=0x15;e->sound_positional=1;return 1;
    }
    rc=smn64_web_consume(resource,s->animation==139?300:900,rng,&e->resource);if(rc<0)return rc;
    if(!rc){e->result_flags=1;return 1;}
    rc=h->bone(h->context,0,e->muzzle);if(rc!=1)return -2;
    rc=h->bone(h->context,1,point);if(rc!=1)return -2;
    for(i=0;i<3;i++)e->muzzle[i]=add(e->muzzle[i],sar(sub(point[i],e->muzzle[i]),1));
    if(smn64_fireweb_angles(e->muzzle,e->target,e->angles)<0)return -2;
    if(resource->web_type){e->special_web_type=(uint32_t)resource->web_type;e->sound=0x80dc;e->sound_positional=1;return 1;}
    e->spawn_impact=1;e->impact_damage=50;e->impact_lifetime=s->animation==139?30:120;e->impact_speed=32;e->impact_special=special_id;e->sound=0x15;e->sound_positional=1;return 1;
}
