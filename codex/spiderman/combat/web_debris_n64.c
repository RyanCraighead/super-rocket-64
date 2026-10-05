#include "web_debris_n64.h"
#include "../web/resource_n64.h"
#include <math.h>
#include <string.h>
static int32_t s32(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)~x;}
static int32_t sar(int32_t v,unsigned n){return s32(((uint32_t)v>>n)|(v<0?(UINT32_MAX<<(32-n)):0));}
static int16_t s16(uint32_t x){x&=65535;return x<=INT16_MAX?(int16_t)x:(int16_t)(-1-(int32_t)(65535-x));}
int smn64_web_debris_init(SmN64WebDebris *s,const int32_t vertices[3][3],const int32_t center[3],
    int32_t floor_y,int32_t speed,uint8_t mode,uint32_t rng[3]){
    int32_t delta[3],velocity[3]={0,0,0};uint32_t sq=0,distance,branch;unsigned i,j;
    if(!s||!vertices||!center||!rng||mode>1)return -1;
    for(i=0;i<3;i++){
        int32_t native;delta[i]=s32((uint32_t)vertices[1][i]-(uint32_t)center[i]);
        /* AC7A8 sends ASR12 into the original signed16 square registers. */
        native=s16((uint32_t)sar(delta[i],12));sq+=(uint32_t)native*(uint32_t)native;
    }
    distance=(uint32_t)sqrtf((float)sq);
    if(distance)for(i=0;i<3;i++)velocity[i]=s32((uint32_t)delta[i]*(uint32_t)speed)/(int32_t)distance;
    memset(s,0,sizeof(*s));memcpy(s->position,vertices,sizeof(s->position));s->floor_y=floor_y;s->shade=128;s->alive=1;
    for(i=0;i<3;i++)for(j=0;j<3;j++)s->velocity[i][j]=velocity[j];
    if(!mode)for(i=0;i<3;i++)s->velocity[i][1]=s32((uint32_t)s->velocity[i][1]+((smn64_web_random(rng,21)-10u)<<12));
    branch=smn64_web_random(rng,3);s->fade_step=(uint8_t)(smn64_web_random(rng,3)+(branch?6:1));return 1;
}
int smn64_web_debris_tick(SmN64WebDebris *s){
    unsigned i,j;if(!s)return -1;if(!s->alive)return 0;
    for(i=0;i<3;i++){
        for(j=0;j<3;j++)s->position[i][j]=s32((uint32_t)s->position[i][j]+(uint32_t)s->velocity[i][j]);
        if(s->position[i][1]>s->floor_y){s->position[i][1]=s->floor_y;for(j=0;j<3;j++)s->velocity[i][j]=sar(s->velocity[i][j],1);}
        s->velocity[i][1]=s32((uint32_t)s->velocity[i][1]+0x7390u);
    }
    s->shade=s->shade<s->fade_step?0:(uint8_t)(s->shade-s->fade_step);if(!s->shade)s->alive=0;return s->alive;
}
