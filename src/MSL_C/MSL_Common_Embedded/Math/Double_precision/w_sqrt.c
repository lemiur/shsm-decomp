// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_sqrt.c
/* Unit name unconfirmed: fdlibm w_sqrt.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * sqrt
 *
 * Wrapper: square root.
 */
double sqrt(double x) {
    return __ieee754_sqrt(x);
} // 0x801A60E0
