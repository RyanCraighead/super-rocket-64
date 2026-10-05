#include "effects_n64.h"
#include "../web/resource_n64.h"
#include "../movement/locomotion_n64.h"
#include <string.h>
uint32_t smn64_combo_start_sound(uint32_t rng[3]){return rng?smn64_web_random(rng,4)+10:0;}
int smn64_combat_hit_particles(uint32_t style,uint16_t animation,uint32_t web_type,uint32_t gloves,
    uint32_t rng[3],SmN64HitEffects *e){
    unsigned i,j;
    if(!rng||!e||style>2)return -1;
    memset(e,0,sizeof(*e));e->style=style;
    if(web_type&&gloves&&(animation==100||animation==102||animation==104||animation==106)){
        e->special_count=10;
        for(i=0;i<10;i++){
            SmN64SpecialHitParticle *p=&e->special[i];uint32_t angle,speed;
            p->size=(uint16_t)(smn64_web_random(rng,200)+350);
            angle=smn64_web_random(rng,4096);speed=smn64_web_random(rng,10)+10;
            p->velocity[0]=(int32_t)speed*smn64_locomotion_sin((int32_t)angle);
            p->velocity[2]=(int32_t)speed*smn64_locomotion_cos((int32_t)angle);
            p->velocity[1]=-(int32_t)((smn64_web_random(rng,20)+20)*4096);
            p->spin=smn64_web_random(rng,2)?-500:500;
        }
    }
    for(i=0;i<2;i++)e->ring_rotation[i]=(uint16_t)smn64_web_random(rng,4096);
    e->particle_count=style?12:6;
    for(i=0;i<e->particle_count;i++){
        uint32_t speed=smn64_web_random(rng,3)+8;
        for(j=0;j<3;j++)e->particles[i].velocity[j]=((int32_t)smn64_web_random(rng,4096)-2048)*(int32_t)speed;
        e->particles[i].life=(uint16_t)(smn64_web_random(rng,7)+12);
    }
    return 1;
}
int smn64_combat_hit_effects(uint16_t animation,uint32_t web_type,uint32_t gloves,
    uint32_t rng[3],SmN64HitEffects *e){
    uint32_t style,sound;
    if(!rng||!e)return -1;
    if(animation==106||animation==113){sound=17;style=2;}
    else if(animation==104||animation==111){sound=16;style=1;}
    else {sound=smn64_web_random(rng,2)+14;style=0;}
    if(smn64_combat_hit_particles(style,animation,web_type,gloves,rng,e)<0)return -1;
    e->sound=sound;return 1;
}
int smn64_impact_debris(uint32_t count,uint32_t rng[3],SmN64ImpactDebris out[30]){
    unsigned i,j,k;if(count>30||!rng||!out)return -1;
    for(i=0;i<count;i++){
        uint32_t branch;
        for(j=0;j<2;j++)for(k=0;k<3;k++)out[i].endpoint_offset[j][k]=((int32_t)smn64_web_random(rng,21)-10)*4096;
        branch=smn64_web_random(rng,3);out[i].size_class=(uint8_t)(smn64_web_random(rng,3)+(branch?6:1));
    }
    return 1;
}
