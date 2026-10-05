#ifndef SM64_SPIDERMAN_DOME_HOST_H
#define SM64_SPIDERMAN_DOME_HOST_H
#include "../../codex/spiderman/controller/dome_owner_n64.h"
#include "../../codex/spiderman/controller/combat_owner_n64.h"
#include "../../codex/spiderman/combat/dome_shatter_n64.h"
#include "../pc/spiderman_dome_runtime.h"
#define SPIDERMAN_DOME_SHARD_CAPACITY 512u
#define SPIDERMAN_DOME_SHAKE_CAPACITY 16u
typedef struct SpidermanDomeShake {uint32_t kind;int32_t position[3];} SpidermanDomeShake;
/* Requires ready hash-verified original geometry/texture renderer. */
int spiderman_dome_host_enable(void);
void spiderman_dome_host_reset(void);
void spiderman_dome_host_disable(void);
const SmN64CombatOwnerServices *spiderman_dome_host_services(void);
int spiderman_dome_host_begin(void);
void spiderman_dome_host_abort(void);
int spiderman_dome_host_validate(void);
void spiderman_dome_host_finalize(void);
/* Actor phase BEFORE global graphics. Exact current pending source frame. */
int spiderman_dome_host_misc(SmN64CombatOwnerFrame *,SmN64CharacterCombat *);
/*6701C hook AFTER sprites5534, BEFORE polygons5540. Runs impacts554C,pulse5530
 * then shatter5540 newest-first. Existing decals5540 have no RNG/allocation,
 * so their relative simulation order has no observable shared dependency.
 * Rendering must still merge both5540 groups by descending shared serial. */
int spiderman_dome_host_graphics(SmN64CombatOwnerFrame *,SmN64CharacterCombat *);
int spiderman_dome_host_snapshot(SpidermanDomeInstance *,size_t,size_t *);
int spiderman_dome_host_shards(SmN64DomeShatterFragment *,size_t,size_t *);
/* Immutable one-frame requests. Native mapping is published only postcommit. */
int spiderman_dome_host_shakes(SpidermanDomeShake *,size_t,size_t *);
#ifdef SPIDERMAN_TESTING
int spiderman_dome_host_test_snapshot(SmN64DomeOwner *);
#endif
#endif
