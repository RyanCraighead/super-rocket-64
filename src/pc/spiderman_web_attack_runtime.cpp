#include "spiderman_web_attack_runtime.h"
#include "../../codex/spiderman/web_sprite/sprite_projection_n64.h"
#include <algorithm>
#include <cfenv>
#include <cstring>
#ifdef __FAST_MATH__
#error "Original attack submission requires no fast-math and FP contraction disabled"
#endif
namespace {
void copy(const SmN64SpriteQuad&s,SpidermanWebQuad&q){
    for(int i=0;i<4;++i){std::copy(s.xyz[i],s.xyz[i]+3,q.xyz[i]);std::copy(s.st[i],s.st[i]+2,q.st[i]);std::copy(s.rgba[i],s.rgba[i]+4,q.rgba[i]);}
    std::copy(s.indices,s.indices+6,q.indices);std::copy(s.model_s16_16,s.model_s16_16+16,q.model_s16_16);q.texture_slot=s.texture_slot;
}
int sprite(const int32_t position[3],int16_t size,int16_t angle,float divisor,uint16_t slot,const uint8_t rgb[3],const float camera[16],uint8_t env,SpidermanWebAttackQuad*out){
    SmN64SpriteInput in={};std::copy(position,position+3,in.position_fixed12);in.size56=size;in.angle84=angle;in.divisor80=divisor;in.texture_width=in.texture_height=32;in.texture_slot=slot;std::copy(rgb,rgb+3,in.logical_rgb);in.alpha=255;
    SmN64SpriteQuad source;SpidermanWebAttackQuad result={};if(!smn64_sprite_project(&in,camera,&source))return -1;copy(source,result.submitted);result.environment_alpha=env;*out=result;return 1;
}
int16_t narrow(uint32_t bits){bits&=65535u;return bits<=32767u?int16_t(bits):int16_t(-1-int32_t(65535u-bits));}
int16_t decalCoordinate(int32_t fixed){uint32_t shifted=uint32_t(fixed)>>13;if(fixed<0)shifted|=0xfff80000u;return narrow(shifted);}
int16_t lineCoordinate(int32_t fixed){const float whole=float(fixed)*(1.0f/4096),render=whole*.0625f;const float rounded=render>0?render+.5f:render-.5f;return narrow(uint32_t(int32_t(rounded)));}
void line(const int32_t a[3],const int32_t b[3],uint8_t shade_a,uint8_t shade_b,SpidermanWebAttackLine&out){
    for(int k=0;k<3;++k){out.xyz[0][k]=lineCoordinate(a[k]);out.xyz[1][k]=lineCoordinate(b[k]);}std::fill(out.rgba[0],out.rgba[0]+4,shade_a);std::fill(out.rgba[1],out.rgba[1]+4,shade_b);
}
}
extern "C" int spiderman_web_attack_snapshot_projectile(const SmN64ImpactWeb*s,const float camera[16],SpidermanWebAttackQuad*out){if(!s||!camera||!out)return -1;if(!s->alive)return 0;const uint8_t rgb[3]={128,128,128};return sprite(s->position,s->size,s->rotation,700,38,rgb,camera,0,out);}
extern "C" int spiderman_web_attack_snapshot_burst(const SmN64ImpactBurst*s,const float camera[16],uint8_t env,SpidermanWebAttackQuad*out){if(!s||!camera||!out)return -1;if(!s->alive)return 0;return sprite(s->position,s->size,0,400,111,s->rgb,camera,env,out);}
extern "C" int spiderman_web_attack_snapshot_decal(const SmN64ImpactDecal*s,SpidermanWebAttackQuad*out){
    if(!s||!out)return -1;
    if(!s->alive)return 0;
    SpidermanWebAttackQuad result={};auto&q=result.submitted;q.texture_slot=108;q.model_s16_16[0]=q.model_s16_16[5]=q.model_s16_16[10]=8192;q.model_s16_16[15]=65536;
    const unsigned order[4]={1,0,2,3};const int16_t uv[4][2]={{0,0},{1024,0},{1024,1024},{0,1024}};const uint8_t indices[6]={0,1,2,0,2,3};
    for(int i=0;i<4;++i){for(int k=0;k<3;++k){q.xyz[i][k]=decalCoordinate(s->corners[order[i]][k]);q.rgba[i][k]=uint8_t(std::min(255u,unsigned(s->rgb[k])*2));}q.rgba[i][3]=uint8_t(2*((unsigned(s->rgb[0])-1)&127u));std::copy(uv[i],uv[i]+2,q.st[i]);}
    std::copy(indices,indices+6,q.indices);*out=result;return 1;
}
extern "C" int spiderman_web_attack_snapshot_fragment(const SmN64WebDebris*s,SpidermanWebAttackLine out[2]){
    if(!s||!out||std::fegetround()!=FE_TONEAREST)return -1;
    if(!s->alive)return 0;
    SpidermanWebAttackLine result[2]={};line(s->position[1],s->position[2],s->shade,s->shade,result[0]);line(s->position[1],s->position[0],s->shade,s->shade,result[1]);std::copy(result,result+2,out);return 1;
}
extern "C" int spiderman_web_attack_snapshot_spark(const SmN64ImpactSpark*s,SpidermanWebAttackLine*out){if(!s||!out||std::fegetround()!=FE_TONEAREST)return -1;if(!s->alive)return 0;SpidermanWebAttackLine result={};line(s->position,s->previous,96,0,result);*out=result;return 1;}
