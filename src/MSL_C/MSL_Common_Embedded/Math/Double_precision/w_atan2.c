// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_atan2.c
/* Unit name unconfirmed: fdlibm w_atan2.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * atan2
 *
 * Wrapper: arc tangent of y/x.
 */
double atan2(double x, double y) {
    return __ieee754_atan2(x, y);
} // 0x801A5E7C
