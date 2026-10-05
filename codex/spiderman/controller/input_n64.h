#ifndef SMN64_INPUT_H
#define SMN64_INPUT_H
#include <stdint.h>
/* Original physical N64 axes -> player logical1123/1124. X clamps68 with
 * seven-unit deadzone/divisor61; Y clamps70/deadzone7/divisor63. Source float32
 * normalizers feed -Y*127 into logicalX, X*127 into logicalY, truncating to s8.
 * These are NOT raw host X/Y. Digital fallback and hardware button profiles
 * are separate boundaries. Output pointers may alias input storage. */
void smn64_input_axes(int8_t raw_x,int8_t raw_y,int8_t logical[2]);
#endif
