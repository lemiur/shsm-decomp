/* Python's text formatting of numbers and objects, byte for byte as CPython 3.14: repr(float), the % operator, the
 * format-spec mini-language of str.format / f-strings / format(), round(), repr(str) and the container reprs, and
 * the arithmetic whose results reach the output (floor division and modulo, sum() and math.fsum over floats).
 * The digits come from exact big-integer arithmetic, not from the C runtime: the C printf rounds the same
 * way for most values but spells inf/nan, exponents, %x of negatives and %g differently, and the project's
 * outputs must not depend on the C library.
 *
 * ---- floats ----
 * avp_float_repr is repr(x) == str(x): the shortest digit string that reads back as the same double (when two
 * shortest strings exist, the one closest to the exact value), written positionally for 1e-4 <= |x| < 1e16 and as
 * d.ddde+XX otherwise (the exponent has a sign and at least two digits: 1e-05, 1e+16), "1.0" for integral values,
 * "inf", "-inf", "nan" (a NaN never prints a sign), "-0.0".
 * avp_float_to_string is PyOS_double_to_string: types e E f F g G r (r ignores the precision and is the repr mode).
 * avp_dtoa is _Py_dg_dtoa (modes 0, 2, 3): the digit string without trailing zeros and the decimal point position.
 *
 * ---- % formatting (avp_pyfmt and friends) ----
 * The format string follows Python's % operator: %[flags][width][.precision][modifier]type with flags - + space # 0,
 * a width and precision that are digits or *, and the modifiers h l L (accepted and ignored, as Python does).
 * %% is a percent sign. There is no %(name) form. Python's rules apply to the output (not C's): the result of
 * '%d' % -5, '%x' % -255 is "-ff" (and "-0xff" with #), '%o' with # gives "0o10", '%c' of a code point gives that
 * character, %f/%e/%g of inf and nan give "inf" and "nan" ("INF"/"NAN" for the upper-case types, a sign flag still
 * adds "+" and the 0 flag pads with zeros: "00inf"), %.0f rounds half to even on the exact binary value
 * ('%.0f' % 0.5 == '0', '%.0f' % 2.5 == '2'), %g drops trailing zeros unless # is given, a 0 flag pads numbers
 * with zeros after the sign and the 0x prefix and is ignored with the - flag, and widths and precisions of %s
 * count code points.
 *
 * The C arguments follow this convention (the macros avp_pyfmt, avp_pyprintf, avp_pyfprintf and avp_buf_pyfmt
 * widen the integer arguments for you, so an int, a u32, a size_t, a bool or a long all work; the va_list functions
 * avp_pyvfmt and avp_buf_pyvfmt take the widened types as listed here):
 *   %d %i            long long (a signed integer)
 *   %u               unsigned long long (decimal)
 *   %x %X %o        long long, signed as Python's ints are: -255 prints "-ff". A u64 above INT64_MAX needs the
 *                    U modifier: %Ux %UX %Uo %Ud take unsigned long long (an extension to Python's syntax)
 *   %c               long long: the code point, written as UTF-8 (a character, not a byte)
 *   %f %F %e %E %g %G double
 *   %v               double printed as str(float) (what Python's %s gives for a float); flags, width and precision
 *                    act on that text as they do for %s ('%.3s' % 1.5 == '1.5')
 *   %s               const char * (UTF-8, NUL-terminated); NULL prints "None"
 *   %S               a const AvpStr * (the address: a struct cannot go through the widening macros; write &s)
 *   %r %R            repr() of a const char * / of a const AvpStr *: quotes and escapes as repr(str)
 *   *                width or precision taken from the arguments (an integer; a negative width means - and its
 *                    absolute value)
 * A malformed format string or a missing/mistyped argument is a programming error (avp_panic).
 *
 * ---- format specs (avp_format_*) ----
 * [[fill]align][sign][z][#][0][width][grouping][.precision][type] exactly as Python's format(): align < > ^ =,
 * sign + - space, grouping , or _ (3 digits; _ groups binary/octal/hex by 4), z (no negative zero), width counted in
 * code points. Integer types b c d n o x X and none (also e E f F g G % with the int converted to float, as
 * format(5, '.2f')); float types e E f F g G n % and none (repr when there is no precision; with a precision
 * like g but with at least one digit after the point: format(1e5, '.6') == '1e+05'); str types s and none.
 * An invalid spec is Python's ValueError: avp_throw(AVP_ERR_OTHER, <Python's message>), fatal if no AVP_TRY is
 * active. 'n' is 'd'/'g' (the C locale, as the tools run).
 *
 * ---- round() ----
 * avp_py_round is round(x, ndigits): correctly rounded half-even on the exact binary value (round(2.675, 2) ==
 * 2.67). avp_py_round_int is round(x) for a float: half-even to an integer (the C rounding of x.5 is away from zero).
 *
 * ---- repr of str and containers ----
 * avp_repr_str is repr(str): single quotes unless the text has a ' and no ", \\ \n \r \t and \x.. \u.... \U........
 * escapes for the non-printable code points (the table is Python 3.14's str.isprintable), the printable non-ASCII
 * ones kept. The container helpers take the already-built reprs of the items.
 *
 * ---- arithmetic ----
 * Python's // and % round toward minus infinity and % takes the sign of the divisor; a zero divisor is Python's
 * ZeroDivisionError (avp_throw). avp_fsum_neumaier is the built-in sum() over floats of CPython 3.12+ (Neumaier
 * compensated summation, the same bits), avp_math_fsum is math.fsum (exactly rounded). */
#pragma once
#include "str.h"
#include <stdio.h>

/* ---- floats ---- */
size_t avp_float_repr(double x, char out[32]); /* repr(x); returns the length; out is NUL-terminated */
AvpStr avp_repr_f64(AvpArena *a, double x);    /* the same as an AvpStr from the arena */

enum {
    AVP_DTSF_SIGN = 1,      /* always write a sign: "+1.5" (Py_DTSF_SIGN) */
    AVP_DTSF_ADD_DOT_0 = 2, /* integral values get ".0" in g and r modes (Py_DTSF_ADD_DOT_0) */
    AVP_DTSF_ALT = 4,       /* keep the decimal point and the trailing zeros of g (Py_DTSF_ALT) */
    AVP_DTSF_NO_NEG_0 = 8   /* a result that rounds to zero loses its minus sign (Py_DTSF_NO_NEG_0) */
};
/* Appends PyOS_double_to_string(x, type, precision, flags): type is one of e E f F g G r; precision is the number
 * of digits after the point (e f) or of significant digits (g), ignored for r. */
void avp_float_to_string(AvpBuf *b, double x, char type, int precision, int flags);

/* _Py_dg_dtoa: mode 0 is the shortest string that round-trips, mode 2 gives ndigits significant digits (at least 1),
 * mode 3 gives ndigits digits after the decimal point; modes 2 and 3 round half to even on the exact value. buf
 * receives the digits without trailing zeros (no digits at all when mode 3 rounds to zero; "0" for 0.0), NUL
 * terminated; the value is 0.DIGITS * 10**decpt; *neg is the sign bit. x must be finite. Returns the digit count. */
#define AVP_DTOA_BUF 800
size_t avp_dtoa(double x, int mode, int ndigits, char buf[AVP_DTOA_BUF], int *decpt, bool *neg);

/* The digits of v in base 2..16 (lower case, or upper case for hex when upper), most significant first, no sign,
 * no prefix; out needs 64 bytes; returns the count ("0" for 0). */
size_t avp_u64_digits(char *out, u64 v, int base, bool upper);

/* ---- % formatting ---- */
void avp_buf_pyvfmt(AvpBuf *b, const char *fmt, va_list ap);
AvpStr avp_pyvfmt(AvpArena *a, const char *fmt, va_list ap); /* a NUL-terminated string from the arena */
void avp_buf_pyfmt_(AvpBuf *b, const char *fmt, ...);
AvpStr avp_pyfmt_(AvpArena *a, const char *fmt, ...);
size_t avp_pyprintf_(const char *fmt, ...);               /* to stdout; returns the bytes written */
size_t avp_pyfprintf_(FILE *f, const char *fmt, ...);

/* The macros widen the integer arguments to the types above. Up to 16 arguments after the format. */
#define AVP__W(x)                                                                                                    \
    _Generic((x), _Bool: (long long)(x), char: (long long)(x), signed char: (long long)(x),                          \
             unsigned char: (long long)(x), short: (long long)(x), unsigned short: (long long)(x),                   \
             int: (long long)(x), long: (long long)(x), long long: (long long)(x),                                   \
             unsigned int: (unsigned long long)(x), unsigned long: (unsigned long long)(x),                          \
             unsigned long long: (unsigned long long)(x), default: (x))
#define AVP__W1(x) , AVP__W(x)
#define AVP__W2(x, ...) , AVP__W(x) AVP__W1(__VA_ARGS__)
#define AVP__W3(x, ...) , AVP__W(x) AVP__W2(__VA_ARGS__)
#define AVP__W4(x, ...) , AVP__W(x) AVP__W3(__VA_ARGS__)
#define AVP__W5(x, ...) , AVP__W(x) AVP__W4(__VA_ARGS__)
#define AVP__W6(x, ...) , AVP__W(x) AVP__W5(__VA_ARGS__)
#define AVP__W7(x, ...) , AVP__W(x) AVP__W6(__VA_ARGS__)
#define AVP__W8(x, ...) , AVP__W(x) AVP__W7(__VA_ARGS__)
#define AVP__W9(x, ...) , AVP__W(x) AVP__W8(__VA_ARGS__)
#define AVP__W10(x, ...) , AVP__W(x) AVP__W9(__VA_ARGS__)
#define AVP__W11(x, ...) , AVP__W(x) AVP__W10(__VA_ARGS__)
#define AVP__W12(x, ...) , AVP__W(x) AVP__W11(__VA_ARGS__)
#define AVP__W13(x, ...) , AVP__W(x) AVP__W12(__VA_ARGS__)
#define AVP__W14(x, ...) , AVP__W(x) AVP__W13(__VA_ARGS__)
#define AVP__W15(x, ...) , AVP__W(x) AVP__W14(__VA_ARGS__)
#define AVP__W16(x, ...) , AVP__W(x) AVP__W15(__VA_ARGS__)
#define AVP__PICK(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, name, ...) name
#define AVP__WIDEN(...)                                                                                              \
    __VA_OPT__(AVP__PICK(__VA_ARGS__, AVP__W16, AVP__W15, AVP__W14, AVP__W13, AVP__W12, AVP__W11, AVP__W10, AVP__W9, \
                         AVP__W8, AVP__W7, AVP__W6, AVP__W5, AVP__W4, AVP__W3, AVP__W2, AVP__W1)(__VA_ARGS__))
#define avp_pyfmt(a, fmt, ...) avp_pyfmt_((a), (fmt) AVP__WIDEN(__VA_ARGS__))
#define avp_buf_pyfmt(b, fmt, ...) avp_buf_pyfmt_((b), (fmt) AVP__WIDEN(__VA_ARGS__))
#define avp_pyprintf(fmt, ...) avp_pyprintf_((fmt) AVP__WIDEN(__VA_ARGS__))
#define avp_pyfprintf(f, fmt, ...) avp_pyfprintf_((f), (fmt) AVP__WIDEN(__VA_ARGS__))

/* ---- format specs: the spec is the text after the colon, "" for none ---- */
AvpStr avp_format_i64(AvpArena *a, i64 v, const char *spec);
AvpStr avp_format_u64(AvpArena *a, u64 v, const char *spec);
AvpStr avp_format_f64(AvpArena *a, double v, const char *spec);
AvpStr avp_format_str(AvpArena *a, AvpStr s, const char *spec);
void avp_buf_format_i64(AvpBuf *b, i64 v, const char *spec);
void avp_buf_format_u64(AvpBuf *b, u64 v, const char *spec);
void avp_buf_format_f64(AvpBuf *b, double v, const char *spec);
void avp_buf_format_str(AvpBuf *b, AvpStr s, const char *spec);

/* ---- round() ---- */
double avp_py_round(double x, int ndigits); /* round(x, ndigits); an overflow gives +-inf where Python raises */
i64 avp_py_round_int(double x);             /* round(x) for |x| < 2**63 (larger values saturate); x must be finite */

/* ---- repr ---- */
bool avp_cp_isprintable(u32 cp); /* str.isprintable for one code point (Python 3.14's Unicode tables) */
bool avp_str_isprintable(AvpStr s);
AvpStr avp_repr_str(AvpArena *a, AvpStr s);
void avp_buf_repr_str(AvpBuf *b, AvpStr s);
AvpStr avp_repr_i64(AvpArena *a, i64 v);
AvpStr avp_repr_bool(bool v);                                   /* "True" / "False" */
AvpStr avp_repr_list(AvpArena *a, const AvpStr *items, size_t n);  /* "[a, b]", "[]" */
AvpStr avp_repr_tuple(AvpArena *a, const AvpStr *items, size_t n); /* "(a, b)", "(a,)" for one item, "()" */
AvpStr avp_repr_set(AvpArena *a, const AvpStr *items, size_t n);   /* "{a, b}", "set()" for none */
AvpStr avp_repr_dict(AvpArena *a, const AvpStr *keys, const AvpStr *vals, size_t n); /* "{k: v, ...}", "{}" */

/* ---- arithmetic with Python's semantics ---- */
i64 avp_floordiv_i64(i64 a, i64 b); /* a // b (b == 0: ZeroDivisionError; INT64_MIN // -1 saturates) */
i64 avp_mod_i64(i64 a, i64 b);      /* a % b: the sign of b */
double avp_floordiv_f64(double a, double b); /* a // b for floats */
double avp_floormod_f64(double a, double b); /* a % b for floats (the sign of b; a zero result takes b's sign) */
/* sum(values): an int 0 start, then the floats added with Neumaier compensation as CPython 3.12+ (so sum([-0.0])
 * is 0.0); avp_sum_f64 is sum(values, start) for a float start. */
double avp_fsum_neumaier(const double *v, size_t n);
double avp_sum_f64(const double *v, size_t n, double start);
/* math.fsum: exactly rounded sum; "-inf + inf in fsum" (ValueError) and "intermediate overflow in fsum"
 * (OverflowError) are avp_throw(AVP_ERR_OTHER, ...). */
double avp_math_fsum(const double *v, size_t n);
/* math.hypot(*v) / math.dist: CPython 3.12+'s vector_norm (the last bit differs from the C library's hypot for some
 * inputs); inf wins over nan, any nan gives nan, no arguments gives 0.0. */
double avp_math_hypot(const double *v, size_t n);
