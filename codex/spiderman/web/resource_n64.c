#include "resource_n64.h"
#include <limits.h>
#include <string.h>
static int32_t si(uint32_t x){return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x);}
static uint32_t sar(uint32_t x,unsigned n){return (x>>n)|((x&0x80000000u)?(UINT32_MAX<<(32-n)):0u);}
uint32_t smn64_web_random(uint32_t s[3],uint32_t n){
    uint32_t v=s[0]*s[1]+s[2];s[0]=v;
    s[1]=(s[1]^v)+sar(v,4);s[2]+=0xefefeff0u+sar(v,3);
    return sar((v&65535u)*n,16);
}
int smn64_web_consume(SmN64WebResource *s,int32_t amount,uint32_t rng[3],SmN64WebResourceEvent *e){
    if(!s||!rng||!e)return -1;
    memset(e,0,sizeof(*e));
    if(s->player_2cc||s->infinite_web||s->suit==2||s->suit==3)return 1;
    if(s->difficulty==0)amount=si(sar((uint32_t)amount<<7,12));
    else if(s->difficulty==1)amount=si(sar((uint32_t)amount<<11,12));
    if(amount<s->remaining){s->remaining=si((uint32_t)s->remaining-(uint32_t)amount);return 1;}
    if(s->cartridges){
        s->remaining=si(4096u-((uint32_t)amount-(uint32_t)s->remaining));
        s->cartridges=si((uint32_t)s->cartridges-1u);s->web_type=0;e->sound=0x1e;return 1;
    }
    if(s->allow_empty)return 1;
    if(!s->voice_busy){e->voice_group=0x21;e->voice_variant=(int32_t)smn64_web_random(rng,3)+2;}
    s->web_type=0;return 0;
}

int smn64_web_refill(SmN64WebResource *s,int32_t amount,int16_t health,uint32_t now,uint32_t *tick){
    if(!s||!tick)return -1;
    if(health<=0)return 0;
    int cap=s->suit>=7&&s->suit<=9?2:10;
    if(!(s->remaining<4096&&!s->web_type)&&s->cartridges>=cap)return 0;
    s->remaining=si((uint32_t)s->remaining+(uint32_t)amount);
    if(s->remaining>4096){
        if(s->cartridges<cap){s->cartridges=si((uint32_t)s->cartridges+1u);s->remaining=si((uint32_t)s->remaining-4096u);*tick=now;}
        else s->remaining=4096;
    }
    return 1;
}
int smn64_web_resource_tick(SmN64WebResource *s,int16_t health,int32_t dt,uint32_t now,uint32_t *tick){
    if(!s||!tick)return -1;
    if(s->cartridges)return 1;
    if(s->web_type){if(s->remaining<512)s->web_type=0;}
    else if(s->remaining<512)smn64_web_refill(s,si((uint32_t)dt<<2),health,now,tick);
    else if(s->remaining<1365)smn64_web_refill(s,dt,health,now,tick);
    return 1;
}

int smn64_web_inventory_start(int32_t difficulty,uint8_t suit,uint32_t level,int32_t saved_remaining,int32_t saved_cartridges,SmN64WebStartInventory *out){
    if(!out||difficulty<0||difficulty>3)return -1;
    const int16_t hp[4]={600,200,100,80},field[4]={100,8,5,3};
    const int32_t carts[4]={9,9,7,2};
    SmN64WebStartInventory s={4096,carts[difficulty],hp[difficulty],field[difficulty]};
    if(suit>=7&&suit<=9)s.cartridges=2;
    if(saved_remaining||saved_cartridges){s.remaining=saved_remaining;s.cartridges=saved_cartridges;}
    if((level>>8)>=9){s.remaining=4096;s.cartridges=2;}
    *out=s;return 1;
}
