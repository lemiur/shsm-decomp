// Reference: bfbbdecomp/bfbb include/rwsdk/rwplcore.h
// Reference: tgsm/pbg include/rwsdk/baimage.h
#ifndef RWSDK_PLCORE_BACOLOR_H
#define RWSDK_PLCORE_BACOLOR_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bamath.h>
#include <rwsdk/plcore/batypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwRGBAReal: a colour with real (floating-point) channels. */
typedef struct RwRGBAReal {
    RwReal red;     /* 0x00 */
    RwReal green;   /* 0x04 */
    RwReal blue;    /* 0x08 */
    RwReal alpha;   /* 0x0C */
} RwRGBAReal;

#define RwRGBARealAssign(_target, _source) (*(_target) = *(_source))

/* Channel-wise sum, difference and scale of real colours (statement macros). */
#define RwRGBARealAdd(o, a, b)                      \
    MACRO_START {                                   \
        (o)->red = (((a)->red) + ((b)->red));       \
        (o)->green = (((a)->green) + ((b)->green)); \
        (o)->blue = (((a)->blue) + ((b)->blue));    \
        (o)->alpha = (((a)->alpha) + ((b)->alpha)); \
    } MACRO_STOP

#define RwRGBARealSub(o, a, b)                      \
    MACRO_START {                                   \
        (o)->red = (((a)->red) - ((b)->red));       \
        (o)->green = (((a)->green) - ((b)->green)); \
        (o)->blue = (((a)->blue) - ((b)->blue));    \
        (o)->alpha = (((a)->alpha) - ((b)->alpha)); \
    } MACRO_STOP

#define RwRGBARealScale(o, a, scale)                \
    MACRO_START {                                   \
        (o)->red = (((a)->red) * (scale));          \
        (o)->green = (((a)->green) * (scale));      \
        (o)->blue = (((a)->blue) * (scale));        \
        (o)->alpha = (((a)->alpha) * (scale));      \
    } MACRO_STOP

/* RwRGBA: an 8-bit-per-channel colour. */
typedef struct RwRGBA {
    RwUInt8 red;    /* 0x0 */
    RwUInt8 green;  /* 0x1 */
    RwUInt8 blue;   /* 0x2 */
    RwUInt8 alpha;  /* 0x3 */
} RwRGBA;

#define RwRGBAAssign(_target, _source) (*(_target) = *(_source))

/* Conversions between 8-bit and real colours (statement macros); the 8-bit side rounds. */
#define RwRGBAFromRwRGBAReal(o, i)                                                  \
    MACRO_START {                                                                   \
        RwInt32 quantize;                                                           \
        quantize = RwInt32FromRealMacro(((i)->red * (RwReal)255.0) + (RwReal)0.5);  \
        (o)->red = (RwUInt8)quantize;                                               \
        quantize = RwInt32FromRealMacro(((i)->green * (RwReal)255.0) + (RwReal)0.5);\
        (o)->green = (RwUInt8)quantize;                                             \
        quantize = RwInt32FromRealMacro(((i)->blue * (RwReal)255.0) + (RwReal)0.5); \
        (o)->blue = (RwUInt8)quantize;                                              \
        quantize = RwInt32FromRealMacro(((i)->alpha * (RwReal)255.0) + (RwReal)0.5);\
        (o)->alpha = (RwUInt8)quantize;                                             \
    } MACRO_STOP

#define RwRGBARealFromRwRGBA(o, i)                                       \
    MACRO_START {                                                        \
        (o)->red = (((RwReal)(((i)->red))) * ((RwReal)((1.0 / 255.0))));     \
        (o)->green = (((RwReal)(((i)->green))) * ((RwReal)((1.0 / 255.0)))); \
        (o)->blue = (((RwReal)(((i)->blue))) * ((RwReal)((1.0 / 255.0))));   \
        (o)->alpha = (((RwReal)(((i)->alpha))) * ((RwReal)((1.0 / 255.0)))); \
    } MACRO_STOP

/*
 * RwSurfaceProperties: how a surface reflects ambient, specular and diffuse light
 * (bfbbdecomp/bfbb rwplcore.h).
 */
typedef struct RwSurfaceProperties {
    RwReal ambient;     /* 0x0 */
    RwReal specular;    /* 0x4 */
    RwReal diffuse;     /* 0x8 */
} RwSurfaceProperties;

void* _rwColorOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwColorClose(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
