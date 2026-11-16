/* Unit name unconfirmed: no public decomp or DOL string checked yet gives this file's name. */

#include <MSL_C/MSL_Common/math_float.h>

/*
 * __signbitd
 *
 * Returns the sign bit of a double, unshifted.
 */
int __signbitd(double x) {
    return __HI(x) & 0x80000000;
} // 0x80199980

/*
 * __fpclassifyd
 *
 * Classifies a double by its exponent field and mantissa.
 */
int __fpclassifyd(double x) {
    switch (__HI(x) & 0x7FF00000) {
    case 0x7FF00000:
        if ((__HI(x) & 0x000FFFFF) || __LO(x)) {
            return FP_NAN;
        }
        return FP_INFINITE;
    case 0:
        if ((__HI(x) & 0x000FFFFF) || __LO(x)) {
            return FP_SUBNORMAL;
        }
        return FP_ZERO;
    }
    return FP_NORMAL;
} // 0x80199998
