#ifndef RWSDK_TOOL_QUAT_RTQUAT_H
#define RWSDK_TOOL_QUAT_RTQUAT_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bavector.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RtQuat: a quaternion, as its imaginary vector and its real part. */
typedef struct RtQuat RtQuat;
struct RtQuat {
    RwV3d imag;     /* 0x0 */
    RwReal real;    /* 0xC */
};

/*
 * result = q1 * q2: the real part, then the cross product of the imaginary parts and each
 * imaginary part scaled by the other's real part. Assumes q1 != result != q2.
 */
#define RtQuatMultiplyMacro(result, q1, q2)                                 \
    do {                                                                    \
        (result)->real = (q1)->real * (q2)->real                            \
                       - RwV3dDotProductMacro(&(q1)->imag, &(q2)->imag);    \
        RwV3dCrossProductMacro(&(result)->imag, &(q1)->imag, &(q2)->imag);  \
        RwV3dIncrementScaledMacro(&(result)->imag, &(q2)->imag, (q1)->real); \
        RwV3dIncrementScaledMacro(&(result)->imag, &(q1)->imag, (q2)->real); \
    } while (0)

#define RtQuatMultiply(result, q1, q2) RtQuatMultiplyMacro(result, q1, q2)

/* The squared length of a quaternion. */
#define RtQuatModulusSquaredMacro(q) \
    ((q)->real * (q)->real + RwV3dDotProductMacro(&(q)->imag, &(q)->imag))

/* result = 1 / q, left unchanged when q has no length. Assumes result != q. */
#define RtQuatReciprocalMacro(result, q)                          \
    do {                                                          \
        RwReal val = RtQuatModulusSquaredMacro(q);                \
                                                                  \
        if (val > (RwReal)0) {                                    \
            val = ((RwReal)1) / val;                              \
            (result)->real = (q)->real * val;                     \
            val = -val;                                           \
            RwV3dScaleMacro(&(result)->imag, &(q)->imag, val);    \
        }                                                         \
    } while (0)

#define RtQuatReciprocal(result, q) RtQuatReciprocalMacro(result, q)

/*
 * Sets mpMatrix to the rotation of a unit quaternion and marks it orthonormal.
 */
#define RtQuatUnitConvertToMatrixMacro(qpQuat, mpMatrix)  \
    do {                                                  \
        const RwReal x = (qpQuat)->imag.x;                \
        const RwReal y = (qpQuat)->imag.y;                \
        const RwReal z = (qpQuat)->imag.z;                \
        const RwReal w = (qpQuat)->real;                  \
        RwV3d square;                                     \
        RwV3d cross;                                      \
        RwV3d wimag;                                      \
                                                          \
        square.x = x * x;                                 \
        square.y = y * y;                                 \
        square.z = z * z;                                 \
                                                          \
        cross.x = y * z;                                  \
        cross.y = z * x;                                  \
        cross.z = x * y;                                  \
                                                          \
        wimag.x = w * x;                                  \
        wimag.y = w * y;                                  \
        wimag.z = w * z;                                  \
                                                          \
        (mpMatrix)->right.x = 1 - 2 * (square.y + square.z); \
        (mpMatrix)->right.y = 2 * (cross.z + wimag.z);    \
        (mpMatrix)->right.z = 2 * (cross.y - wimag.y);    \
                                                          \
        (mpMatrix)->up.x = 2 * (cross.z - wimag.z);       \
        (mpMatrix)->up.y = 1 - 2 * (square.x + square.z); \
        (mpMatrix)->up.z = 2 * (cross.x + wimag.x);       \
                                                          \
        (mpMatrix)->at.x = 2 * (cross.y + wimag.y);       \
        (mpMatrix)->at.y = 2 * (cross.x - wimag.x);       \
        (mpMatrix)->at.z = (1 - 2 * (square.x + square.y)); \
                                                          \
        (mpMatrix)->pos.x = ((RwReal)0);                  \
        (mpMatrix)->pos.y = ((RwReal)0);                  \
        (mpMatrix)->pos.z = ((RwReal)0);                  \
                                                          \
        rwMatrixSetFlags((mpMatrix), (rwMATRIXTYPEORTHONORMAL & ~rwMATRIXINTERNALIDENTITY)); \
    } while (0)

#define RtQuatUnitConvertToMatrix(qpQuat, mpMatrix) RtQuatUnitConvertToMatrixMacro(qpQuat, mpMatrix)

/* Sets qpQuat from the rotation of an orthonormal matrix. Returns FALSE for a NULL argument. */
RwBool RtQuatConvertFromMatrix(RtQuat* qpQuat, const RwMatrix* mpMatrix);

/*
 * Rotates (or sets) quat by angle degrees about axis, as combineOp says: replace the quaternion,
 * or concatenate the rotation before or after it. Returns NULL on error.
 */
RtQuat* RtQuatRotate(RtQuat* quat, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp);

/* Rotates numPoints vectors by the unit quaternion quat. */
RwV3d* RtQuatTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, const RwInt32 numPoints,
                              const RtQuat* quat);

#ifdef __cplusplus
}
#endif

#endif
