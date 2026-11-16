#ifndef _MSL_ANSI_FP_H
#define _MSL_ANSI_FP_H

#ifdef __cplusplus
extern "C" {
#endif

#define SIGDIGLEN 36

typedef struct decimal {
    char sign;
    char unk1;
    short exp;
    struct {
        unsigned char length;
        unsigned char text[SIGDIGLEN];
        unsigned char unk41;
    } sig;
} decimal;

typedef struct decform {
    char style;
    char unk1;
    short digits;
} decform;

void __ull2dec(decimal* result, unsigned long long val);
void __timesdec(decimal* result, const decimal* x, const decimal* y);
void __str2dec(decimal* d, const char* s, short exp);
void __two_exp(decimal* result, long exp);
int __equals_dec(const decimal* x, const decimal* y);
int __less_dec(const decimal* x, const decimal* y);
void __minus_dec(decimal* z, const decimal* x, const decimal* y);
void __num2dec_internal(decimal* d, double x);
void __num2dec(const decform* form, double x, decimal* d);
double __dec2num(const decimal* d);

#ifdef __cplusplus
}
#endif

#endif
