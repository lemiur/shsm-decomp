// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_acos.c
/* Unit name unconfirmed: fdlibm w_acos.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * acos
 *
 * Wrapper: arc cosine.
 */
double acos(double x) {
    return __ieee754_acos(x);
} // 0x801A5E74
