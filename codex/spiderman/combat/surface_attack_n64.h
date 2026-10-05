#ifndef SMN64_SURFACE_ATTACK_H
#define SMN64_SURFACE_ATTACK_H
#include "air_attack_n64.h"
/* Original9996C: wall/ceiling attack enters the same aerial owner without the
 * ordinary airborne downward-angle or target-type exclusions. Source keeps
 * wall/ceiling orientation flags but clearsCF0. No trail allocation here. */
int smn64_surface_attack_request(SmN64AirAttack *,uint32_t *surface_mode,
    uint32_t wall,uint32_t ceiling,uint32_t target_id,const int32_t target[3],
    uint8_t punch_pressed,uint8_t kick_pressed,uint32_t tick,
    const uint16_t *counts,size_t,SmN64AirEvent *);
#endif
