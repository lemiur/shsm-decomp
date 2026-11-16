#ifndef _MSL_MATH_FLOAT_H
#define _MSL_MATH_FLOAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Classes returned by __fpclassifyd. */
#define FP_NAN 1
#define FP_INFINITE 2
#define FP_ZERO 3
#define FP_NORMAL 4
#define FP_SUBNORMAL 5

/* High and low words of a double (big endian). */
#define __HI(x) (*(int*)&(x))
#define __LO(x) (*(1 + (int*)&(x)))

int __signbitd(double x);
int __fpclassifyd(double x);

#ifdef __cplusplus
}
#endif

#endif
