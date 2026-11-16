// Reference: zeldaret/tp libs/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common/Src/float.c
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MSL_C/MSL_Common/float.c
//
// Bit patterns of the special floating-point values behind math.h's NAN, HUGE_VALF/INFINITY and HUGE_VAL.
#include <math.h>

unsigned long __float_nan[] = { 0x7FFFFFFF };
unsigned long __float_huge[] = { 0x7F800000 };
unsigned long __double_huge[] = { 0x7FF00000, 0 };
