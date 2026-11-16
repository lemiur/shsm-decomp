// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Double_precision/s_copysign.c
/* Unit name unconfirmed: fdlibm s_copysign.c, as in the public decomps; no DOL string names the file. */

#include <math.h>

/*
 * copysign
 *
 * Returns x with the sign bit of y.
 */
double copysign(double x, double y) {
    __HI(x) = (__HI(x) & 0x7fffffff) | (__HI(y) & 0x80000000);
    return x;
} // 0x801A58F8
