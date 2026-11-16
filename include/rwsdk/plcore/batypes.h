// Reference: tgsm/pbg include/rwsdk/plcore/bavector.h
#ifndef RWSDK_PLCORE_BATYPES_H
#define RWSDK_PLCORE_BATYPES_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RwSplitBits: a real and its bit pattern. The integer members are volatile as in the SDK header,
 * so every access to them goes through memory.
 */
typedef union RwSplitBits {
    RwReal nReal;
    volatile RwInt32 nInt;      // volatile: RwSplitBits as in the RenderWare SDK; retail reloads every nInt access from the stack
    volatile RwUInt32 nUInt;    // volatile: RwSplitBits as in the RenderWare SDK; retail reloads every nUInt access from the stack
} RwSplitBits;

/* RwV2d: a 2D vector. */
typedef struct RwV2d {
    RwReal  x;
    RwReal  y;
} RwV2d;

/* RwV3d: a 3D vector or point. */
typedef struct RwV3d {
    RwReal  x;
    RwReal  y;
    RwReal  z;
} RwV3d;

/* RwSphere: a sphere, its centre and radius. */
typedef struct RwSphere {
    RwV3d center;   /* 0x0 */
    RwReal radius;  /* 0xC */
} RwSphere;

/* RwPlane: a plane, its unit normal and its distance from the origin along the normal. */
typedef struct RwPlane {
    RwV3d normal;       /* 0x0 */
    RwReal distance;    /* 0xC */
} RwPlane;

/* RwTexCoords: the u and v texture coordinates of a vertex. */
typedef struct RwTexCoords {
    RwReal u;   /* 0x0 */
    RwReal v;   /* 0x4 */
} RwTexCoords;

/* The bracket of a statement macro; the guards are the SDK header's. */
#ifndef MACRO_START
#define MACRO_START do
#endif
#ifndef MACRO_STOP
#define MACRO_STOP while (0)
#endif

/* RwFixed: a 16.16 fixed-point number, and the SDK header's conversions. */
typedef RwInt32 RwFixed;

#define RwFixedToInt(a) ((a) >> 16)
#define RwFixedToReal(a) ((RwReal)(((RwReal)(a)) * (1.0f / 65536.0f)))
#define RwIntToFixed(a) ((a) << 16)
#define RwRealToFixed(a) (RwInt32FromRealMacro((a) * 65536.0f))

/* Pi and a right angle in radians; the guard is the SDK header's. */
#ifndef rwPI
#define rwPI ((RwReal)(3.1415926535f))
#define rwPIOVER2 (rwPI / (RwReal)(2.0f))
#endif

#ifdef __cplusplus
}
#endif

#endif
