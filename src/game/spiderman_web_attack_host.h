#ifndef SM64_SPIDERMAN_WEB_ATTACK_HOST_H
#define SM64_SPIDERMAN_WEB_ATTACK_HOST_H
#include "../../codex/spiderman/controller/combat_owner_n64.h"
#include "../../codex/spiderman/controller/web_attack_effects_n64.h"
/* Optional capability. Enable only after source materials38/108/111 and actual
 * untextured-line submissions are verified and installed. Initially gated. */
int spiderman_web_attack_host_enable(void);
void spiderman_web_attack_host_reset(void);
void spiderman_web_attack_host_disable(void);
/* Wrap native combat services, retaining every unrelated existing callback.
 * All new callbacks operate only during a begun whole-source transaction. */
const SmN64CombatOwnerServices *spiderman_web_attack_host_services(void);
int spiderman_web_attack_host_begin(void);
void spiderman_web_attack_host_abort(void);
/* Precommit must succeed BEFORE publishing native recipients. It validates and
 * seals the completed pending effect pass. Finalize performs only in-memory
 * publication, cannot fail, and is a no-op without successful validation.
 * No effects/service call may mutate a sealed transaction. */
int spiderman_web_attack_host_validate(void);
void spiderman_web_attack_host_finalize(void);
/* Convenience for standalone transactions, never call this after irreversible
 * native publication: use validate-before-native + finalize-after-native. */
int spiderman_web_attack_host_commit(void);
/* Run before traversal strand jitter (source5534 precedes5530). This explicitly
 * binds the current pending frame for the real native actor query and clears it
 * before returning. Call before committing source, native recipients or RNG. */
int spiderman_web_attack_host_effects(SmN64CombatOwnerFrame *,SmN64CharacterCombat *);
/* The gap hook runs after5534 and before5540 while the CURRENT source frame,
 * recipient transaction and attack registry are bound. Its clock points to the
 * shared source clock, synchronized with all preceding projectile allocations. */
int spiderman_web_attack_host_effects_interleaved(SmN64CombatOwnerFrame *,
    SmN64CharacterCombat *,SmN64WebAttackBetweenPasses,void *context);
int spiderman_web_attack_host_snapshot(SmN64WebAttackObject *,size_t,size_t *);
/* Actual dynamic-only geometry boundary used after source ordered actor sweep.
 * 0 means no intersected dynamic triangle; -4 means an actual unsupported
 * dynamic actor-mesh contact; -3 corrupt/unbounded partitions. Static triangles
 * never hide a dynamic contact. No invented native recipient mapping. */
int spiderman_web_attack_dynamic_only(const int32_t from[3],const int32_t to[3]);
#endif
