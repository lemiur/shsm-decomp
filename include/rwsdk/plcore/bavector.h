// Reference: tgsm/pbg include/rwsdk/plcore/bavector.h
#ifndef RWSDK_PLCORE_BAVECTOR_H
#define RWSDK_PLCORE_BAVECTOR_H

#include <rwsdk/plcore/batypes.h>
#include <rwsdk/plcore/bamatrix.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Vector arithmetic (bfbbdecomp/bfbb rwplcore.h). */
#define RwV3dAddMacro(o, a, b)                      do {                                                (o)->x = (((a)->x) + ((b)->x));                 (o)->y = (((a)->y) + ((b)->y));                 (o)->z = (((a)->z) + ((b)->z));             } while (0)

#define RwV3dSubMacro(o, a, b)                      do {                                                (o)->x = (((a)->x) - ((b)->x));                 (o)->y = (((a)->y) - ((b)->y));                 (o)->z = (((a)->z) - ((b)->z));             } while (0)

#define RwV3dScaleMacro(o, a, s)                    do {                                                (o)->x = (((a)->x) * ((s)));                    (o)->y = (((a)->y) * ((s)));                    (o)->z = (((a)->z) * ((s)));                } while (0)

#define RwV3dIncrementScaledMacro(o, a, s)          do {                                                (o)->x += (((a)->x) * ((s)));                   (o)->y += (((a)->y) * ((s)));                   (o)->z += (((a)->z) * ((s)));               } while (0)

#define RwV3dNegateMacro(o, a)                      do {                                                (o)->x = -(a)->x;                               (o)->y = -(a)->y;                               (o)->z = -(a)->z;                           } while (0)

#define RwV3dDotProductMacro(a, b)     ((((((((a)->x) * ((b)->x))) + ((((a)->y) * ((b)->y))))) + ((((a)->z) * ((b)->z)))))

/*
 * Scales _in to unit length into _out and sets _result to the reciprocal of its length.
 */
#define _rwV3dNormalizeMacro(_result, _out, _in)                                        do {                                                                                    RwReal length2 = RwV3dDotProductMacro(_in, _in);                                    rwInvSqrtMacro(&(_result), length2);                                                RwV3dScaleMacro(_out, _in, _result);                                            } while (0)

#define RwV3dCrossProductMacro(o, a, b)                                             do {                                                                                (o)->x = (((((a)->y) * ((b)->z))) - ((((a)->z) * ((b)->y))));                   (o)->y = (((((a)->z) * ((b)->x))) - ((((a)->x) * ((b)->z))));                   (o)->z = (((((a)->x) * ((b)->y))) - ((((a)->y) * ((b)->x))));               } while (0)

/* The transforms of one point or vector, and of an array of them, by a matrix. */
typedef RwV3d* (*rwVectorMultFn)(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix);
typedef RwV3d* (*rwVectorsMultFn)(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints,
                                  const RwMatrix* matrix);

RwBool _rwVectorSetMultFn(rwVectorMultFn multPoint, rwVectorsMultFn multPoints,
                          rwVectorMultFn multVector, rwVectorsMultFn multVectors);

/*
 * Transforms numPoints points by matrix (paired-single assembly in bavector.c; tgsm/pbg's name for
 * the function at this place).
 */
RwV3d* RwV3dTransformPoints(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints, const RwMatrix* matrix);

/* Transforms one vector (no translation) by matrix (paired-single assembly in bavector.c; name unknown). */
RwV3d* RwV3dTransformVector(RwV3d* vectorOut, const RwV3d* vectorIn, const RwMatrix* matrix);

/* Transforms one point by matrix (paired-single assembly in bavector.c; name unknown). */
RwV3d* RwV3dTransformPoint(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix);

RwReal _rwSqrt(const RwReal num);
RwReal _rwInvSqrt(const RwReal num);
RwReal _rwV3dNormalize(RwV3d* out, const RwV3d* in);
RwReal RwV3dNormalize(RwV3d* out, const RwV3d* in);
RwReal RwV3dLength(const RwV3d* in);
RwReal RwV2dLength(const RwV2d* in);
RwReal RwV2dNormalize(RwV2d* out, const RwV2d* in);

void* _rwVectorOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwVectorClose(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
