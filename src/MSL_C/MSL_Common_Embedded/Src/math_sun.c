// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Src/math_sun.c
/* Unit name unconfirmed: MSL math_sun.c, as in the public decomps; no DOL string names the file. */

#include <math.h>

/*
 * scalbn
 *
 * Returns x * 2^n by splitting off the binary exponent and rebuilding the
 * number with the adjusted one.
 */
double scalbn(double x, int n) {
    double mant;
    int exp;

    mant = frexp(x, &exp);
    exp += n;
    return ldexp(mant, exp);
} // 0x801A1D98
