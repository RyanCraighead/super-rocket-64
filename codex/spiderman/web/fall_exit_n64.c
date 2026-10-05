#include "fall_exit_n64.h"
static int retained_air_clip(uint16_t clip){
    switch(clip){
        case 211:case 212:case 224:case 225:case 231:case 232:
        case 226:case 228:case 233:case 235:case 175:case 176:
        case 127:case 238:case 241:case 240:case 239:case 215:
        case 216:case 221:case 218:case 222:case 219:case 278:
        case 276:return 1;
        default:return 0;
    }
}
int smn64_web_fall_exit(SmN64FallExit *out,const uint16_t *counts,size_t count){
    if(!out||!counts)return -1;
    SmN64FallExit s=*out;
    int crossed=(((uint32_t)s.velocity_y^(uint32_t)s.previous_velocity_y)&0x80000000u)!=0;
    uint16_t old=s.anim.animation,next=65535;
    if(crossed||!retained_air_clip(old)||(s.launch_flag&&s.velocity_y>0)){
        s.falling_origin_y=s.position_y;s.falling_tick=s.now;
        if(old!=175&&old!=176){
            if(s.launch_flag){next=old==280?278:276;s.launch_flag=0;}
            else switch(old){
                case 231:next=232;break;case 224:next=225;break;
                case 218:next=219;break;case 221:next=222;break;
                case 215:next=216;break;case 232:case 216:break;
                default:next=212;break;
            }
        }
    }
    if(next!=65535){
        if(next>=count||!counts[next])return -1;
        smn64_anim_run(&s.anim,next,counts[next],0,-1);
    }
    if(s.d20&&(uint32_t)(s.now-s.falling_tick)>=31u)s.d20=s.d24=0;
    *out=s;return 1;
}
