#ifndef MSL_MATH_H
#define MSL_MATH_H

#include <MSL_C/MSL_Common/math_float.h>

#ifdef __cplusplus
extern "C" {
#endif

float sqrtf(float x);
float sinf(float x);
double acos(double x);
double asin(double x);
double atan(double x);
double atan2(double y, double x);
double cos(double x);
double sin(double x);
double tan(double x);
double exp(double x);
double log(double x);
double fmod(double x, double y);
double ceil(double x);
double floor(double x);
double sqrt(double x);
double scalbn(double x, int n);
double nan(const char* tagp);

inline float cosf(float x) { return cos(x); }
inline double fabs(double x) { return __fabs(x); }
inline float fabsf(float x) { return fabs(x); }
inline long double fabsl(long double x) { return __fabs(x); }

extern unsigned long __float_nan[];
extern unsigned long __float_huge[];
extern unsigned long __double_huge[];

#define NAN (*(float*)__float_nan)

#define HUGE_VALF (*(float*)__float_huge)
#define HUGE_VAL (*(double*)__double_huge)
#define INFINITY (*(float*)__float_huge)

#define fpclassify(x) __fpclassifyd(x)
#define signbit(x) __signbitd(x)
#define isfinite(x) (fpclassify(x) > FP_INFINITE)
#define isnan(x) (fpclassify(x) == FP_NAN)

double copysign(double x, double y);
double frexp(double x, int* exp);
double ldexp(double x, int exp);
double log10(double x);
double pow(double x, double y);

inline float powf(float x, float y) { return pow(x, y); }

#ifdef __cplusplus
}
#endif

#endif
