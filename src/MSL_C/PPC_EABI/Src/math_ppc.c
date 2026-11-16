// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/PPC_EABI/Src/math_ppc.c
/* Unit name unconfirmed: MSL math_ppc.c, as in the public decomps; no DOL string names the file. */

#include <math.h>

/*
 * nan
 *
 * MSL's PowerPC build leaves the body empty: no quiet NaN is built, and the
 * caller gets whatever f1 held (R36, the original's quirk).
 */
double nan(const char* tagp) {
} // 0x801A60DC
