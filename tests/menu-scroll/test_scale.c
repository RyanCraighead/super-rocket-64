#include <assert.h>
#include <math.h>
#include "pc/djui/djui.h"
#include "pc/configfile.h"
unsigned int configDjuiScale;
static u32 w,h;
void gfx_get_dimensions(u32*x,u32*y){*x=w;*y=h;}
f32 clamp(f32 v,f32 lo,f32 hi){return fminf(hi,fmaxf(lo,v));}
int main(void){
    unsigned dims[][2]={{320,240},{640,360},{640,480},{800,1280},{1280,720},{1920,1080},{2560,1080}};
    for(unsigned d=0;d<7;d++)for(configDjuiScale=0;configDjuiScale<5;configDjuiScale++){
        w=dims[d][0];h=dims[d][1];float s=djui_gfx_get_scale();
        assert(s>=.5&&s<=1.5);assert(w/s>=320);assert(h/s>=240);
    }
    return 0;
}
