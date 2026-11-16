// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MSL_C/MSL_Common_Embedded/Math/Double_precision/w_log.c
/* Unit name unconfirmed: fdlibm w_log.c, as in the public decomps; no DOL string names the file. */

#include <MSL_C/MSL_Common_Embedded/Math/fdlibm.h>

/*
 * log
 *
 * Wrapper: natural logarithm.
 */
double log(double x) {
    return __ieee754_log(x);
} // 0x801A5E88
