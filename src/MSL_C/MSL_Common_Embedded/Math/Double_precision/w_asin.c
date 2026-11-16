// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_asin.c
/* Unit name unconfirmed: fdlibm w_asin.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * asin
 *
 * Wrapper: arc sine.
 */
double asin(double x) {
    return __ieee754_asin(x);
} // 0x801A5E78
