// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_pow.c
/* Unit name unconfirmed: fdlibm w_pow.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * pow
 *
 * Wrapper: power.
 */
double pow(double x, double y) {
    return __ieee754_pow(x, y);
} // 0x801A5E90

