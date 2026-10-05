#include "combat_n64.h"
#include <math.h>
#include <string.h>

static int32_t s32(uint32_t v) { return v<=INT32_MAX?(int32_t)v:-1-(int32_t)~v; }
static int32_t sub(int32_t a,int32_t b) { return s32((uint32_t)a-(uint32_t)b); }
static int32_t add(int32_t a,int32_t b) { return s32((uint32_t)a+(uint32_t)b); }
static int32_t sar(int32_t a,unsigned n) {
    uint32_t v=(uint32_t)a;if(!n)return a;
    return s32((v>>n)|(a<0?(UINT32_MAX<<(32-n)):0));
}
static uint16_t le16(const uint8_t **p) { uint16_t v=(uint16_t)((*p)[0]|((uint16_t)(*p)[1]<<8));*p+=2;return v; }
static int valid(const SmN64CombatBank *b,uint16_t id) { return b&&id<32&&b->combo[id].present; }
static int anim_valid(uint16_t a,const uint16_t *c,size_t n) { return c&&a<n&&c[a]&&c[a]<=INT16_MAX; }
int smn64_combat_bank_load(SmN64CombatBank *out,const uint8_t *data,size_t size) {
    static const uint8_t header[72]={
      'S','M','N','6','4','C','0','2',
0xfe,0xff,0x90,0xed,0x12,0x01,0xc9,0x1f,0xf1,0x67,0xd6,0x69,0x58,0x04,0x8e,0x61,0xc1,0x92,0xc9,0xd6,0xa7,0x56,0xdd,0xb9,0x8f,0x79,0x90,0x17,0xac,0x9c,0xd2,0x5c,0x1d,0x3e,0xd3,0x38,0x4f,0x45,0xad,0xa2,0xeb,0xf6,0xcb,0x06,0x66,0xdd,0xc7,0xfe,0xc4,0xc4,0xff,0xdb,0x6f,0xdb,0x99,0x3b,0x8d,0x56,0x6a,0xa3,0xcd,0x4f,0x38,0x67};
    SmN64CombatBank b;const uint8_t *p;unsigned i,j;
    if(!out||!data||size!=97077||memcmp(data,header,sizeof(header)))return -1;
    memset(&b,0,sizeof(b));p=data+sizeof(header);
    for(i=0;i<32;i++) {
        SmN64ComboDef *d=&b.combo[i];uint16_t f[18];
        for(j=0;j<18;j++)f[j]=le16(&p);
        d->present=f[0];d->id=f[1];d->animation=f[2];d->damage=f[3];d->hit_start=f[4];d->hit_end=f[5];
        d->normal_start=f[6];d->normal_end=f[7];d->alternate_start=f[8];d->alternate_end=f[9];
        d->impulse=f[10];d->duration=f[11];d->hit_flags=f[12];d->alternate=f[13];
        d->branch_count=f[14];d->sequence_count=f[15];d->bone_count=f[16];d->frame_count=f[17];
        if(d->present>1||d->branch_count>16||d->sequence_count>128||d->bone_count>32||d->frame_count>256)return -1;
        if(d->present&&(d->id!=i||d->animation>=300||!d->frame_count))return -1;
        for(j=0;j<16;j++) { SmN64ComboBranch *q=&d->branches[j];
            q->target=le16(&p);q->at=le16(&p);q->offset=le16(&p);q->animation=le16(&p);q->flag=le16(&p);
            if(j<d->branch_count&&(q->target>=32||q->animation>=300))return -1;
        }
        memcpy(d->sequence,p,128);p+=128;memcpy(d->bones,p,32);p+=32;memcpy(d->frames,p,256);p+=256;
        for(j=0;j<d->sequence_count;j+=j?2:1)if(d->sequence[j]>3)return -1;
    }
    for(i=0;i<300;i++) {b.root[i].count=le16(&p);if(b.root[i].count>256)return -1;memcpy(b.root[i].values,p,256);p+=256;}
    for(i=0;i<32;i++)for(j=0;j<b.combo[i].branch_count;j++)if(!valid(&b,b.combo[i].branches[j].target))return -1;
    memcpy(b.air_types,p,21);
    for(i=0;i<21;i++)if(b.air_types[i]>2)return -1;
    *out=b;return 1;
}
int smn64_combo_begin(SmN64Combo *s,const SmN64CombatBank *b,uint16_t id,uint32_t tick,
                     uint16_t offset,const uint16_t *counts,size_t count) {
    const SmN64ComboDef *d;unsigned i;int32_t elapsed=(int32_t)offset;
    if(!s||!valid(b,id))return -1;
    d=&b->combo[id];if(!anim_valid(d->animation,counts,count)||elapsed/2>=d->frame_count)return -1;
    if(d->frames[elapsed/2]>=counts[d->animation])return -1;
    memset(s->touched,0,sizeof(s->touched));s->touched_count=0;
    s->transition_animation=0;s->root_cursor=0;
    s->id=id;s->active=1;s->matching=d->branch_count!=0;s->pending_id=-1;
    s->started=tick-offset;s->last_input=tick;s->first_pose=1;
    for(i=0;i<d->branch_count;i++) {
        const SmN64ComboBranch *q=&d->branches[i];SmN64ComboCandidate *c=&s->candidate[i];
        c->active=1;c->first=1;c->alternate=b->combo[q->target].alternate!=0;c->flags=q->animation?0x300:0;
        c->web_prefix=0;c->cursor=0;
    }
    smn64_anim_run(&s->anim,d->animation,counts[d->animation],d->frames[elapsed/2],-1);
    return 1;
}
static void root_motion(SmN64Combo *s,const SmN64CombatBank *b,int32_t at,SmN64CombatMotion *m) {
    const SmN64RootDef *r;uint32_t i;
    if(s->anim.animation>=300)return;
    r=&b->root[s->anim.animation];if(!r->count)return;
    for(i=s->root_cursor;i<r->count&&(int32_t)i<=at;i++) {
        int32_t v=r->values[i];
        if(v>=-128&&v<=-121) {
            static const uint8_t bones[4]={6,5,1,0};
            m->anchored=1;m->anchor_bone=bones[(unsigned)(v+128)%4];m->include_y=v<-124;
        } else {m->anchored=0;m->forward_units=add(m->forward_units,v);}
    }
    /* The FF terminator is retained as the cursor when it is reached. */
    s->root_cursor=(uint16_t)i;
}
int smn64_combo_tick_admitted(SmN64Combo *s,const SmN64CombatBank *b,SmN64CombatInput *input,
                    uint32_t tick,const uint16_t *counts,size_t count,SmN64ComboResult *out,
                    SmN64ComboMotionResolve resolve,void *context,SmN64CombatAdmission admit,void *admit_context) {
    const SmN64ComboDef *d;int32_t elapsed,at;uint32_t pressed,allowed=0;unsigned i;int transition;uint16_t old_cursor;int16_t old_frame;
    if(!s||!input||!out||!valid(b,s->id)||!s->active)return -1;
    old_cursor=s->root_cursor;old_frame=s->anim.frame;
    memset(out,0,sizeof(*out));d=&b->combo[s->id];elapsed=s32(tick-s->started);at=elapsed/2;
    if(elapsed<0)return -1;
    transition=s->transition_animation&&s->anim.animation==s->transition_animation;
    if(!transition) {
        if(at>=d->frame_count) {
            input->pressed=0;s->active=0;
            if(s->pending_id>=0) {
                int id=s->pending_id;int rc=smn64_combo_begin(s,b,(uint16_t)id,tick,s->pending_offset,counts,count);
                if(rc<0)return rc;
                out->status=id;out->began=1;
            }
            return 1;
        }
        if(!anim_valid(s->anim.animation,counts,count)||d->frames[at]>=counts[s->anim.animation])return -1;
        s->anim.frame=d->frames[at];
    }
    root_motion(s,b,transition?s->anim.frame:at,&out->motion);
    if(resolve&&(out->motion.anchored||out->motion.forward_units)){int rc=resolve(context,s,&out->motion);if(rc<0){s->root_cursor=old_cursor;s->anim.frame=old_frame;return rc;}}
    pressed=input->pressed&15u;input->pressed=0;if(pressed)s->last_input=tick;
    if(s->matching) {
        if(pressed) {
            for(i=0;i<d->branch_count;i++) {
                SmN64ComboCandidate *c=&s->candidate[i];
                const SmN64ComboDef *next=&b->combo[d->branches[i].target];
                uint32_t bit;uint16_t first=c->alternate?d->alternate_start:d->normal_start;
                uint16_t last=c->alternate?d->alternate_end:d->normal_end;
                if(!c->active||elapsed<first||elapsed>last)continue;
                if(c->cursor>=next->sequence_count)return -1;
                bit=1u<<next->sequence[c->cursor];allowed|=bit;
                if(!(pressed&bit))continue;
                if(bit==1)c->web_prefix=1;
                else if(c->web_prefix&&!input->web_held){c->active=0;continue;}
                if(c->first){c->first=0;c->last_tick=tick;c->cursor++;}
                else {
                    if(c->cursor+1>=next->sequence_count)return -1;
                    if(tick-c->last_tick>next->sequence[c->cursor+1])c->active=0;
                    else {c->last_tick=tick;c->cursor+=2;}
                }
                if(c->cursor>=next->sequence_count) {
                    const SmN64ComboBranch *q=&d->branches[i];
                    SmN64CombatCommand command=smn64_combo_command(q->target);
                    if(command&&admit) {
                        int accepted=admit(admit_context,command);
                        if(accepted<0||accepted>1)return -2;
                        if(!accepted){c->active=0;s->matching=0;break;}
                    }
                    s->pending_id=(int16_t)q->target;s->pending_at=q->at;s->pending_offset=q->offset;
                    s->transition_animation=q->animation;s->matching=0;break;
                }
            }
            if(i==d->branch_count&&(allowed|pressed)!=allowed)s->matching=0;
        }
    } else if(elapsed>=d->normal_start&&s->pending_id>=0&&elapsed>=s->pending_at) {
        int ready=1;
        if(s->transition_animation) {
            if(s->anim.animation!=s->transition_animation) {
                if(!anim_valid(s->transition_animation,counts,count))return -1;
                s->root_cursor=0;smn64_anim_run(&s->anim,s->transition_animation,counts[s->transition_animation],0,-1);
                ready=0;
            } else ready=s->anim.finished!=0;
        }
        if(ready) {
            int id=s->pending_id;int rc=smn64_combo_begin(s,b,(uint16_t)id,tick,s->pending_offset,counts,count);
            if(rc<0)return rc;
            out->status=id;out->began=1;return 1;
        }
    }
    out->hit_active=elapsed>=d->hit_start&&elapsed<=d->hit_end;
    out->sample_bones=out->hit_active;out->status=1;return 1;
}
SmN64CombatCommand smn64_combo_command(uint16_t id) {
    return id==2?SMN64_COMMAND_TRAP:id==3?SMN64_COMMAND_YANK:
        id==6?SMN64_COMMAND_IMPACT:SMN64_COMMAND_NONE;
}
int smn64_combo_tick_resolved(SmN64Combo *s,const SmN64CombatBank *b,SmN64CombatInput *input,
    uint32_t tick,const uint16_t *counts,size_t count,SmN64ComboResult *out,
    SmN64ComboMotionResolve resolve,void *context) {
    return smn64_combo_tick_admitted(s,b,input,tick,counts,count,out,resolve,context,NULL,NULL);
}
int smn64_combo_tick(SmN64Combo *s,const SmN64CombatBank *b,SmN64CombatInput *i,
    uint32_t tick,const uint16_t *counts,size_t count,SmN64ComboResult *out){
    return smn64_combo_tick_resolved(s,b,i,tick,counts,count,out,NULL,NULL);
}
/* Matches original single-precision 0x80051C58 normalization order. */
void smn64_combat_normalize(const int32_t src[3],int32_t dst[3]) {
    float v[3],q,a,b,scale;unsigned i;
    for(i=0;i<3;i++)v[i]=(float)src[i]*(1.0f/4096.0f);
    a=v[0]*v[0];b=v[1]*v[1];a=a+b;b=v[2]*v[2];q=sqrtf(a+b);
    if(q==0){memset(dst,0,3*sizeof(*dst));return;}
    scale=1.0f/q;for(i=0;i<3;i++){a=v[i]*scale;a=a*4096.0f;dst[i]=(int32_t)a;}
}
int smn64_combo_contacts(SmN64Combo *s,const SmN64CombatBank *b,uint32_t tick,
                        const SmN64CombatActor *actors,size_t n,uint8_t suit,
                        int32_t difficulty,const SmN64CombatHost *host) {
    const SmN64ComboDef *d;int32_t elapsed,sampled[32][3];unsigned j,k;size_t i;int contacts=0;
    if(!s||!valid(b,s->id)||!host||!host->bone||!host->sweep||!host->apply||(!actors&&n))return -1;
    if(s->touched_count>4)return -1;
    if(!s->active)return 0;
    d=&b->combo[s->id];elapsed=s32(tick-s->started);
    if(elapsed<d->hit_start||elapsed>d->hit_end)return 0;
    for(j=0;j<d->bone_count;j++)
        if(host->bone(host->context,d->bones[j],sampled[j])!=1)return -2;
    for(j=0;j<d->bone_count;j++) {
        if(!s->first_pose)memcpy(s->prior_bones[j],s->bones[j],sizeof(s->bones[j]));
        memcpy(s->bones[j],sampled[j],sizeof(s->bones[j]));
    }
    if(s->first_pose){s->first_pose=0;return 0;}
    for(i=0;i<n;i++) {
        const SmN64CombatActor *a=&actors[i];int already=0;
        if(!(a->flags&2)||a->type==0x13c||a->distance>=700)continue;
        for(k=0;k<s->touched_count;k++)if(a->id==s->touched[k])already=1;
        if(already)continue;
        for(j=0;j<d->bone_count;j++) {
            SmN64CombatHit hit;int32_t mid[3],v[3];int rc;
            memset(&hit,0,sizeof(hit));
            for(k=0;k<3;k++)mid[k]=add(s->bones[j][k],sar(sub(s->bones[j][k],s->prior_bones[j][k]),1));
            rc=host->sweep(host->context,a->id,s->prior_bones[j],mid,8192,&hit.hit_part,hit.position);
            if(rc<0)return -2;
            if(!rc)continue;
            hit.actor=a->id;hit.flags=d->hit_flags?0x4f:0x0f;
            hit.damage=(uint16_t)smn64_damage_scaled(d->damage,suit,difficulty);
            v[0]=sar(sub(mid[0],s->prior_bones[j][0]),12);v[1]=0;v[2]=sar(sub(mid[2],s->prior_bones[j][2]),12);
            smn64_combat_normalize(v,hit.direction);
            if(s->glove_hits&&(s->anim.animation==100||s->anim.animation==102||s->anim.animation==104||s->anim.animation==106)){
                hit.damage=(uint16_t)((uint32_t)hit.damage*2);s->glove_hits--;
            }
            if(d->impulse){hit.flags|=0x10;hit.impulse=d->impulse;hit.duration=d->duration;}
            rc=host->apply(host->context,&hit);if(rc<0)return -2;
            contacts++;if(s->touched_count<4)s->touched[s->touched_count++]=a->id;
            /* Source A0D5C delay slot sets the first-contact flag even when
             * the actor rejects damage. A0E68 stops after this geometric hit. */
            return contacts;
        }
    }
    return contacts;
}
uint32_t smn64_combat_select_target(const SmN64CombatTarget *a,size_t n,int32_t maxd,
                                  int32_t minface,int32_t dw,int32_t fw) {
    uint32_t id=0;int32_t best=0;size_t i;
    if(!a||maxd<=0)return 0;
    for(i=0;i<n;i++) {
        int32_t score,ratio;
        if(!a[i].enabled||(a[i].flags&0x50)!=0x10||a[i].distance>=maxd)continue;
        ratio=s32((uint32_t)sub(maxd,a[i].distance)<<12)/maxd;
        score=sar(s32((uint32_t)ratio*(uint32_t)dw),12);
        if(fw) {
            int32_t angle;if(a[i].facing<minface)continue;
            angle=add(a[i].facing,4096)/2;
            score=add(score,sar(s32((uint32_t)angle*(uint32_t)fw),12));
        }
        if(score>best&&a[i].visible){best=score;id=a[i].id;}
    }
    return id;
}
