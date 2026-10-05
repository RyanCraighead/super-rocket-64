#ifndef SMN64_ACTOR_BRIDGE_H
#define SMN64_ACTOR_BRIDGE_H
#include "../movement/player_n64.h"
#include "../climbing/climbing_n64.h"
#include "../web/owner_n64.h"
/* Synchronize only shared source fields; retain mechanic-specific state and
 * resource/graphic/target lifetimes. No simulation, allocation or host effects.
 * The full climb basis is authoritative; never synthesize it from display yaw.
 * These are explicit transport helpers, not a second controller pass. */
void smn64_bridge_ground_to_actor(const SmN64Player *,SmN64ClimbState *);
void smn64_bridge_actor_to_ground(const SmN64ClimbState *,SmN64Player *);
void smn64_bridge_actor_to_web(const SmN64ClimbState *,SmN64WebRuntime *,SmN64WebOwner *);
void smn64_bridge_web_to_actor(const SmN64WebRuntime *,const SmN64WebOwner *,SmN64ClimbState *);
/* player_finish_with_hooks authoritative basis implementation. Context is the
 * caller's SmN64ClimbState transaction copy. Handles original previous-normal
 * cache semantics and copies exact resulting forward cells to the ground view. */
int smn64_bridge_ground_basis(void *,SmN64Locomotion *);
#endif
