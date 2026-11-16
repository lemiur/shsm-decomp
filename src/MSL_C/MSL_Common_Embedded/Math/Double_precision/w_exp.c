// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_exp.c
/* Unit name unconfirmed: fdlibm w_exp.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * exp
 *
 * Wrapper: exponential.
 */
double exp(double x) {
    return __ieee754_exp(x);
} // 0x801A5E80
