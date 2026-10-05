#ifndef SM64_SPIDERMAN_WEB_ATTACK_RUNTIME_H
#define SM64_SPIDERMAN_WEB_ATTACK_RUNTIME_H
#include "gfx/spiderman_web_attack_gl.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "../../codex/spiderman/controller/web_attack_effects_n64.h"
/* Pure source producers. Return1 live,0 inactive,-1 invalid; non-success leaves
 * outputs unchanged. Source matrix arithmetic requires round-to-nearest and
 * no-fast-math/no-contraction. Caller owns source ENV alpha for the burst. */
int spiderman_web_attack_snapshot_projectile(const SmN64ImpactWeb *,const float native_camera[16],SpidermanWebAttackQuad *);
int spiderman_web_attack_snapshot_burst(const SmN64ImpactBurst *,const float native_camera[16],uint8_t source_environment_alpha,SpidermanWebAttackQuad *);
int spiderman_web_attack_snapshot_decal(const SmN64ImpactDecal *,SpidermanWebAttackQuad *);
/* Fragment emits newer owned line(shared->next) before parent(shared->prior),
 * matching original constructor head insertion. Spark is current->previous. */
int spiderman_web_attack_snapshot_fragment(const SmN64WebDebris *,SpidermanWebAttackLine out[2]);
int spiderman_web_attack_snapshot_spark(const SmN64ImpactSpark *,SpidermanWebAttackLine *);
#ifdef __cplusplus
}
#endif
#endif
