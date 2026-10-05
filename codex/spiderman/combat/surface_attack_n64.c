#include "surface_attack_n64.h"
#include <string.h>
int smn64_surface_attack_request(SmN64AirAttack *s,uint32_t *surface,uint32_t wall,uint32_t ceiling,
    uint32_t target_id,const int32_t target[3],uint8_t punch,uint8_t kick,uint32_t tick,
    const uint16_t *counts,size_t n,SmN64AirEvent *e){
    uint16_t clip=punch?133:129;
    if(!s||!surface||!target||!e)return -1;
    memset(e,0,sizeof(*e));if(s->aiming||(!wall&&!ceiling)||!target_id||(!punch&&!kick))return 0;
    if(!counts||clip>=n||!counts[clip]||counts[clip]>INT16_MAX)return -1;
    s->movement_enabled=0;s->normal[0]=0;s->normal[1]=-4096;s->normal[2]=0;
    *surface=0;s->landing_wait=0;s->target_id=target_id;memcpy(s->target_position,target,sizeof(s->target_position));
    s->state=0x1000000;s->previous_attack_tick=s->attack_tick;s->attack_tick=tick;s->hit_done=0;
    smn64_anim_run(&s->anim,clip,counts[clip],0,-1);e->align_normal=1;e->face_target=1;return 1;
}
