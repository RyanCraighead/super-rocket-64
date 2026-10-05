#ifndef SMN64_COMBAT_COMMAND_H
#define SMN64_COMBAT_COMMAND_H
#include <stdint.h>
/* Typed native capability admission, not source controller bits. Zero masks
 * preserve the unmodified source behavior. Missing dependencies are errors;
 * only an explicitly disabled command may be declined without a transition. */
typedef enum SmN64CombatCommand {
    SMN64_COMMAND_NONE=0,
    SMN64_COMMAND_TRAP=1u<<0,
    SMN64_COMMAND_YANK=1u<<1,
    SMN64_COMMAND_IMPACT=1u<<2,
    SMN64_COMMAND_DOME=1u<<3,
    SMN64_COMMAND_GRAB=1u<<4,
    SMN64_COMMAND_CARRY=1u<<5,
    SMN64_COMMAND_AIM=1u<<6,
    SMN64_COMMAND_MOUNTED=1u<<7,
    SMN64_COMMAND_INTERACT=1u<<8,
    SMN64_COMMAND_GLOVES=1u<<9
} SmN64CombatCommand;
#define SMN64_COMMAND_ALL ((1u<<10)-1u)
/* Pure admission: 1 available, 0 deliberately unavailable, negative malformed
 * policy. Must not call actor/graphics/audio/RNG services. */
typedef int (*SmN64CombatAdmission)(void *,SmN64CombatCommand);
#endif
