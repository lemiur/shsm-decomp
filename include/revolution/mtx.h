#ifndef REVOLUTION_MTX_H
#define REVOLUTION_MTX_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Matrix and vector types of the MTX library; only the types are declared here (GX takes them as arguments). */

typedef struct Vec {
    f32 x;
    f32 y;
    f32 z;
} Vec;

typedef struct Vec2 {
    f32 x;
    f32 y;
} Vec2;

typedef f32 Mtx[3][4];
typedef f32 Mtx44[4][4];
typedef f32 (*MtxPtr)[4];
typedef const f32 (*CMtxPtr)[4];

void PSMTXIdentity(Mtx m);
void PSMTXCopy(const Mtx src, Mtx dst);
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab);
void PSMTXRotRad(Mtx m, char axis, f32 rad);
void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT);
void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT);
void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS);
u32 PSMTXInverse(const Mtx src, Mtx inv); /* 0: the matrix is singular, 1: inverted */
void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst);
void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

#ifdef __cplusplus
}
#endif

#define MTXCopy PSMTXCopy
#define MTXConcat PSMTXConcat
#define MTXRotRad PSMTXRotRad
#define MTXTrans PSMTXTrans
#define MTXTransApply PSMTXTransApply
#define MTXScale PSMTXScale
#define MTXInverse PSMTXInverse

#endif
