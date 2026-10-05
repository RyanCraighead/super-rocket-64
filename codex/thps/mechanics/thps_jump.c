/* Reconstructed from supplied THPS1 N64 USA Rev1, not THPS2.
 * 0x80057040..0x80057230: charge accumulation and saturation.
 * 0x80057668..0x80057BCC: air/ollie stats select Q12 impulse.
 * All division is truncation toward zero as in the original MIPS code.
 */
#include "thps_skate.h"
int32_t thps1_ollie_charge_limit(int32_t air_stat,int32_t ollie_stat) {
    return 15 - (air_stat + ollie_stat) / 2;
}
int32_t thps1_ollie_impulse(int32_t air_stat,int32_t ollie_stat,int32_t charge,int vertical_ramp) {
    int32_t stat=vertical_ramp?air_stat:ollie_stat;
    int32_t limit=thps1_ollie_charge_limit(air_stat,ollie_stat);
    int32_t base=32-(10-stat)/2;
    int32_t rise=base-18+(10-stat)/3;
    if (limit<=0 || charge<0 || charge>limit) return 0;
    return 3*((base*1024)+(charge*rise*1024)/limit);
}
