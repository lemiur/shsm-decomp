// Name, locals and evidence: Gormiti link map and DWARF (RpHAnim.a stdkey.obj): _rwIEEEFloatShapeType,
// the GET_FLOAT_WORD/SET_FLOAT_WORD blocks gf_u and sf_u, and the locals of the expanded bodies
// (all at the line of their use: the bodies are macros).
// Reference: fdlibm e_acosf.c and k_sinf.c (the constants and steps)
// Header and macro names invented: the SDK header this code came from is not known.
#ifndef RWSDK_PLCORE_RWFDLIBM_H
#define RWSDK_PLCORE_RWFDLIBM_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bavector.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * fdlibm's single-precision plumbing, expanded in place by RenderWare's toolkits on the GameCube
 * and Wii: the arc cosine (e_acosf.c, with the square root taken by _rwSqrt and a magnitude of one
 * or more clamped to the nearer end instead of a NaN) and the sine kernel (k_sinf.c, for an angle
 * already in range).
 */

/* A float and its bit pattern. */
typedef union _rwIEEEFloatShapeType {
    float value;
    unsigned int word;
} _rwIEEEFloatShapeType;

#define GET_FLOAT_WORD(i, d)            \
    do {                                \
        _rwIEEEFloatShapeType gf_u;     \
        gf_u.value = (d);               \
        (i) = gf_u.word;                \
    } while (0)

#define SET_FLOAT_WORD(d, i)            \
    do {                                \
        _rwIEEEFloatShapeType sf_u;     \
        sf_u.word = (i);                \
        (d) = sf_u.value;               \
    } while (0)

/* e_acosf.c's constants. */
static const RwReal one = 1.0f;
static const RwReal pi = 3.1415925026e+00f;
static const RwReal pio2_hi = 1.5707962513e+00f;
static const RwReal pio2_lo = 7.5497894159e-08f;
static const RwReal pS0 = 1.6666667163e-01f;
static const RwReal pS1 = -3.2556581497e-01f;
static const RwReal pS2 = 2.0121252537e-01f;
static const RwReal pS3 = -4.0055535734e-02f;
static const RwReal pS4 = 7.9153501429e-04f;
static const RwReal pS5 = 3.4793309169e-05f;
static const RwReal qS1 = -2.4033949375e+00f;
static const RwReal qS2 = 2.0209457874e+00f;
static const RwReal qS3 = -6.8828397989e-01f;
static const RwReal qS4 = 7.7038154006e-02f;

/* result = the arc cosine of x, in radians. x is evaluated several times. */
#define rwACosFMacro(result, x)                                                                \
    do {                                                                                       \
        RwReal df, c, s, w, r, q, p, z;                                                        \
        RwInt32 hx, ix;                                                                        \
                                                                                               \
        GET_FLOAT_WORD(hx, x);                                                                 \
        ix = hx & 0x7FFFFFFF;                                                                  \
        if (ix >= 0x3F800000) { /* |x| >= 1 */                                                 \
            if (hx > 0) {                                                                      \
                (result) = 0.0f;                                                               \
            } else {                                                                           \
                (result) = pi + 2.0f * pio2_lo;                                                \
            }                                                                                  \
        } else if (ix < 0x3F000000) { /* |x| < 0.5 */                                          \
            if (ix <= 0x23000000) { /* |x| < 2**-57 */                                         \
                (result) = pio2_hi + pio2_lo;                                                  \
            } else {                                                                           \
                z = (x) * (x);                                                                 \
                p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));       \
                q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));                         \
                r = p / q;                                                                     \
                (result) = pio2_hi - ((x) - (pio2_lo - (x) * r));                              \
            }                                                                                  \
        } else if (hx < 0) { /* x < -0.5 */                                                    \
            z = 0.5f * (one + (x));                                                            \
            p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));           \
            q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));                             \
            s = _rwSqrt(z);                                                                    \
            r = p / q;                                                                         \
            w = r * s - pio2_lo;                                                               \
            (result) = pi - 2.0f * (s + w);                                                    \
        } else { /* x > 0.5 */                                                                 \
            z = 0.5f * (one - (x));                                                            \
            s = _rwSqrt(z);                                                                    \
            {                                                                                  \
                int idf;                                                                       \
                                                                                               \
                GET_FLOAT_WORD(idf, s);                                                        \
                SET_FLOAT_WORD(df, idf & 0xFFFFF000); /* s with its low 12 mantissa bits cleared */ \
            }                                                                                  \
            c = (z - df * df) / (s + df);                                                      \
            p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));           \
            q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));                             \
            r = p / q;                                                                         \
            w = r * s + c;                                                                     \
            (result) = 2.0f * (df + w);                                                        \
        }                                                                                      \
    } while (0)

/* k_sinf.c's constants. */
static const RwReal S1 = -1.6666667163e-01f;
static const RwReal S2 = 8.3333337680e-03f;
static const RwReal S3 = -1.9841270114e-04f;
static const RwReal S4 = 2.7557314297e-06f;
static const RwReal S5 = -2.5050759689e-08f;
static const RwReal S6 = 1.5896910177e-10f;

/* result = the sine of x for |x| up to about pi/4 (fdlibm's __kernel_sinf with no tail). */
#define rwKernelSinFMacro(result, x)                                \
    do {                                                            \
        RwReal z, r, v;                                             \
                                                                    \
        z = (x) * (x);                                              \
        v = z * (x);                                                \
        r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));           \
        (result) = (x) + v * (S1 + z * r);                          \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
