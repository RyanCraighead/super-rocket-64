/* Original 8006511C source numeric submission. See trail_render/README.md. */
#include "spiderman_trail_runtime.h"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <limits>
#ifdef __FAST_MATH__
#error "Original trail projection requires no fast-math and FP contraction disabled"
#endif
namespace {
// Original TRUNC.W.S then SH, with defined narrowing. Out-of-range conversion
// is not a supported MIPS floating-point exception path: fail closed instead.
bool coordinate(float value,int16_t &out){
    if(!std::isfinite(value)||value < -2147483648.0f||double(value)>=2147483648.0)return false;
    const uint32_t bits=uint32_t(int32_t(value))&65535u;
    out=bits<=32767u?int16_t(bits):int16_t(-1-int32_t(65535u-bits));return true;
}
void position(const int32_t input[3],float output[3]){
    for(int k=0;k<3;++k){const float whole=float(input[k])*(1.0f/4096.0f);output[k]=whole*.0625f;}
}
}
extern "C" int spiderman_trail_snapshot(const SmN64Trail *trail,const float camera[16],SpidermanWebQuad out[4],size_t *count){
    if(!trail||!camera||!out||!count||std::fegetround()!=FE_TONEAREST||trail->head>4)return -1;
    if(trail->delete_requested)return 0;
    for(int k=0;k<16;++k)if(!std::isfinite(camera[k])||std::fabs(camera[k])>1e6f)return -1;
    SpidermanWebQuad result[4]={};size_t n=0;bool connected=false;float previous[3]={};
    for(unsigned i=0;i<4;++i){
        const auto &segment=trail->segment[i];float from[3],to[3],delta[3],unit[3],rotated[3],side[3];
        const bool first=segment.first||!connected;
        if(first)position(segment.from,from);else std::copy(previous,previous+3,from);
        position(segment.to,to);std::copy(to,to+3,previous);
        for(int k=0;k<3;++k)delta[k]=to[k]-from[k];
        const float xx=delta[0]*delta[0],yy=delta[1]*delta[1],zz=delta[2]*delta[2];
        const float xy=xx+yy,sum=xy+zz,length=std::sqrt(sum);
        connected=false;if(length<.001f)continue;
        if(!std::isfinite(length))return -1;
        const float reciprocal=1.0f/length;
        for(int k=0;k<3;++k)unit[k]=delta[k]*reciprocal;
        for(int k=0;k<3;++k){const float a=unit[0]*camera[k*4],b=unit[1]*camera[k*4+1],c=unit[2]*camera[k*4+2];const float ab=a+b;rotated[k]=ab+c;}
        const float ax=rotated[1]*camera[10],bx=rotated[2]*camera[9];side[0]=ax-bx;
        const float ay=rotated[2]*camera[8],by=rotated[0]*camera[10];side[1]=ay-by;
        const float az=rotated[0]*camera[9],bz=rotated[1]*camera[8];side[2]=az-bz;
        const float scale=.5f*length;for(int k=0;k<3;++k)side[k]=side[k]*scale;
        auto &quad=result[n];quad.texture_slot=41;quad.model_s16_16[0]=quad.model_s16_16[5]=quad.model_s16_16[10]=quad.model_s16_16[15]=65536;
        const uint8_t indices[6]={0,2,3,0,3,1};std::copy(indices,indices+6,quad.indices);
        uint8_t rgba[4];for(int k=0;k<3;++k)rgba[k]=uint8_t(std::min(255u,((segment.color>>(k*8))&255u)*2));rgba[3]=std::max(rgba[0],std::max(rgba[1],rgba[2]));
        for(int v=0;v<4;++v){
            if(v<2&&!first){std::copy(result[n-1].xyz[v+2],result[n-1].xyz[v+2]+3,quad.xyz[v]);std::copy(result[n-1].st[v+2],result[n-1].st[v+2]+2,quad.st[v]);std::copy(result[n-1].rgba[v+2],result[n-1].rgba[v+2]+4,quad.rgba[v]);continue;}
            for(int k=0;k<3;++k){const float center=v<2?from[k]:to[k];const float value=(v&1)?center+side[k]:center-side[k];if(!coordinate(value,quad.xyz[v][k]))return -1;}
            quad.st[v][0]=0;quad.st[v][1]=(v&1)?320:0;std::copy(rgba,rgba+4,quad.rgba[v]);
        }
        ++n;connected=true;
    }
    std::copy(result,result+n,out);*count=n;return 1;
}
