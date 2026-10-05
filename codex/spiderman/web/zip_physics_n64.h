#ifndef SMN64_ZIP_PHYSICS_H
#define SMN64_ZIP_PHYSICS_H
#include <stdint.h>

/* Original free-flight zip branch of the supplied Spider-Man USA 1.0 ROM,
 * 80084254..80084324, returning at 80085144. Vectors are signed fixed12,
 * Y down. This branch has no world query and does not synthesize gravity.
 * The owner must select a present swinger first, then adhered motion, before
 * considering this predicate. Animation frame is the original signed s16. */
int smn64_zip_physics_active(uint32_t state, uint16_t clip, int16_t frame);

typedef struct SmN64ZipPhysics {
    int32_t velocity[3];
    int32_t displacement[3];
} SmN64ZipPhysics;

/* Add retained acceleration once, apply arithmetic-shift drag (byte & 31),
 * then clear components in the inclusive [-2048, 2048] deadband. Displacement
 * is the resulting velocity times (signed dt >= 3 ? dt - 1 : 1), retaining
 * the low 32 bits. Thus dt 0, 1 and 2 all move by one velocity, and dt does
 * not scale acceleration. Caller adds displacement to position with wrap32.
 * All pointers must be non-NULL. Inputs may overlap output: all are consumed
 * before output is written. This function does not check the clip predicate.
 *
 * At source 80084200 the owner clears collision E0, D48, side-hit DB8,
 * ceiling-hit EB4 and platform pointer 10B4 before selecting any motion path.
 * The full owner also retains the former platform pointer on its stack.
 * Those fields and the next AI/state update remain the caller's responsibility.
 */
void smn64_zip_physics_step(const int32_t velocity[3],
                           const int32_t acceleration[3],
                           const uint8_t drag[3], int32_t dt,
                           SmN64ZipPhysics *out);
#endif
