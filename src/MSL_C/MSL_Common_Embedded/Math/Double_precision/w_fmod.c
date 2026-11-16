// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_fmod.c
/* Unit name unconfirmed: fdlibm w_fmod.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * fmod
 *
 * Wrapper: floating remainder.
 */
double fmod(double x, double y) {
    return __ieee754_fmod(x, y);
} // 0x801A5E84
