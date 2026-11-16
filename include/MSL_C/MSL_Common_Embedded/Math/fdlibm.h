// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common_Embedded/Math/Include/fdlibm.h
#ifndef _MSL_FDLIBM_H
#define _MSL_FDLIBM_H

/*
 * fdlibm's private interface: the IEEE 754 core routines behind the math.h
 * wrappers and the kernel functions of the trigonometric reductions. __HI and
 * __LO come from math.h (MSL_Common/math_float.h).
 */

#include <errno.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IEEE 754 core routines */
double __ieee754_sqrt(double x);
double __ieee754_acos(double x);
double __ieee754_asin(double x);
double __ieee754_atan2(double y, double x);
double __ieee754_exp(double x);
double __ieee754_fmod(double x, double y);
double __ieee754_log(double x);
double __ieee754_log10(double x);
double __ieee754_pow(double x, double y);
int __ieee754_rem_pio2(double x, double* y);

/* kernel functions of the trigonometric routines */
double __kernel_sin(double x, double y, int iy);
double __kernel_cos(double x, double y);
double __kernel_tan(double x, double y, int k);
int __kernel_rem_pio2(double* x, double* y, int e0, int nx, int prec, const int* ipio2);

#ifdef __cplusplus
}
#endif

#endif
