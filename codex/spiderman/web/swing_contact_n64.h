#ifndef SMN64_SWING_CONTACT_H
#define SMN64_SWING_CONTACT_H
#include "swinger_n64.h"
#include "traversal_n64.h"

typedef enum SmN64SwingContactRay {
    SMN64_SWING_CONTACT_FORWARD=1,
    SMN64_SWING_CONTACT_DOWN=2,
    SMN64_SWING_CONTACT_UP=3
} SmN64SwingContactRay;
typedef struct SmN64SwingContactQuery {
    int32_t start[3], end[3];
    SmN64SwingContactRay kind;
    int32_t arg1, arg2, arg3, arg4; /* original 8004C0B0: 1,0,0,1 */
    uint8_t line_byte_88;           /* original 8004A204: zero */
} SmN64SwingContactQuery;
typedef struct SmN64SwingContact {
    int32_t position[3], velocity[3]; /* player04/64 */
    uint16_t collision, body_offset; /* E0/11A0; normally offset96 */
    int32_t line_start[3], line_end[3]; /* D50/D5C, last issued ray */
    SmN64WebLine line;                 /* retained side-line hit fields */
} SmN64SwingContact;
/* Marker evaluates the original authored marker2 for the supplied body state.
 * It is called once at old position, then again at the stepped swinger endpoint.
 * Its offset depends on pose/body transform; a constant offset is not equivalent.
 * Callbacks return1 on success; unavailable/unsupported queries must not return
 * a fabricated no-hit. Callbacks must be pure and may be retried. */
typedef int (*SmN64SwingMarker)(void *,const SmN64SwingContact *,unsigned marker,
                               int32_t out[3]);
typedef int (*SmN64SwingTrace)(void *,const SmN64SwingContactQuery *,SmN64WebLine *);
/* Original contact owner80085174. The caller has ALREADY advanced the swinger
 * in original actor-update order. Does not divide displacement by elapsed ticks.
 * Returns1 success,-1 invalid arguments,-2 unavailable marker,-3 unavailable ray.
 * Negative returns leave this state unchanged (host transaction boundary). */
int smn64_swing_contact_run(SmN64SwingContact *,const SmN64Swinger *,
                            SmN64SwingMarker,SmN64SwingTrace,void *);
#endif
