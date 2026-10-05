#include "spiderman_web_runtime.h"
#include "../../codex/spiderman/web_sprite/sprite_projection_n64.h"
#include <algorithm>
#include <cstring>
namespace {
int16_t sourceSplatCoordinate(int32_t fixed) {
    // Source63068..630F4: signed SRA13 then SH, not float rounding.
    uint32_t shifted=uint32_t(fixed)>>13;
    if(fixed<0)shifted|=0xfff80000u;
    const uint32_t bits=shifted&65535u;
    return bits<=32767u?int16_t(bits):int16_t(-1-int32_t(65535u-bits));
}
}
extern "C" int spiderman_web_snapshot_splat(const SmN64WebSplat *s,SpidermanWebQuad *out) {
    if(!s||!out)return -1;
    if(!s->alive)return 0;
    SpidermanWebQuad q={};q.texture_slot=47;
    // Source63028..63050: BD06C diag.125 followed by guMtxF2L.
    q.model_s16_16[0]=q.model_s16_16[5]=q.model_s16_16[10]=8192;
    q.model_s16_16[15]=65536;
    const unsigned order[4]={1,0,2,3};
    const int16_t uv[4][2]={{0,0},{1024,0},{1024,1024},{0,1024}};
    for(int i=0;i<4;++i){
        for(int k=0;k<3;++k){q.xyz[i][k]=sourceSplatCoordinate(s->corners[order[i]][k]);q.rgba[i][k]=uint8_t(std::min(255u,unsigned(s->rgb[k])*2));}
        // Source draw overrides stored alpha using packed logical-red low byte.
        q.rgba[i][3]=uint8_t(2*((unsigned(s->rgb[0])-1u)&127u));
        std::copy(uv[i],uv[i]+2,q.st[i]);
    }
    // B9DE8 emits both facing pairs; the host draws one pair with culling off,
    // preserving the two-sided surface without double translucent blending.
    const uint8_t indices[6]={0,1,2,0,2,3};std::copy(indices,indices+6,q.indices);
    *out=q;return 1;
}
extern "C" int spiderman_web_snapshot_knot(const int32_t position[3],const float camera[16],
    const uint8_t rgb[3],uint8_t alpha,SpidermanWebQuad *out) {
    if(!out)return -1;
    SmN64SpriteQuad source;
    if(!smn64_web_knot_quad(position,camera,rgb,alpha,&source))return -1;
    SpidermanWebQuad q={};
    for(int i=0;i<4;++i){std::copy(source.xyz[i],source.xyz[i]+3,q.xyz[i]);std::copy(source.st[i],source.st[i]+2,q.st[i]);std::copy(source.rgba[i],source.rgba[i]+4,q.rgba[i]);}
    std::copy(source.indices,source.indices+6,q.indices);
    std::copy(source.model_s16_16,source.model_s16_16+16,q.model_s16_16);
    q.texture_slot=source.texture_slot;*out=q;return 1;
}
extern "C" int spiderman_web_camera_from_host_view(const float view[16],float out[16]) {
    return smn64_sprite_camera_from_host_view(view,out);
}
