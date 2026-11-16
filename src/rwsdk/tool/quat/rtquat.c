// Names, locals and statement order: Gormiti link map and DWARF (RtQuat.a rtquat.obj).
/*
 * rtquat.c: the quaternion toolkit. It converts a rotation matrix to a quaternion, rotates a
 * quaternion about an axis and rotates arrays of vectors by a quaternion. The toolkit's error
 * records carry rwID_QUATPLUGIN (0x195). RtQuatModulus and RtQuatQueryRotate have no copy in
 * retail (dead-stripped).
 */
#include <math.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/tool/quat/rtquat.h>

#define rwPLUGIN_ID rwID_QUATPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/*
 * ==================== QuatFromPositiveDiagMatrix ====================
 * Builds the quaternion from a matrix whose trace rTR is positive. Inlined into its only caller
 * at retail.
 * Name and locals: Gormiti link map and DWARF (RtQuat.a rtquat.obj).
 */
static RtQuat* QuatFromPositiveDiagMatrix(RtQuat* qpQuat, const RwMatrix* mpMatrix, RwReal rTR) {
    RwReal rS;

    rS = 1.0f + rTR;
    rS = _rwSqrt(rS);
    qpQuat->real = 0.5f * rS;
    rS = 0.5f / rS;
    qpQuat->imag.x = rS * (mpMatrix->up.z - mpMatrix->at.y);
    qpQuat->imag.y = rS * (mpMatrix->at.x - mpMatrix->right.z);
    qpQuat->imag.z = rS * (mpMatrix->right.y - mpMatrix->up.x);

    return qpQuat;
}

/*
 * ==================== QuatFromXDiagDomMatrix ====================
 * The matrix's trace is not positive and right.x is its largest diagonal element.
 * Name and local: Gormiti link map and DWARF (RtQuat.a rtquat.obj).
 */
static RtQuat* QuatFromXDiagDomMatrix(RtQuat* qpQuat, const RwMatrix* mpMatrix) {
    RwReal rS;

    rS = 1.0f + (mpMatrix->right.x - (mpMatrix->up.y + mpMatrix->at.z));
    rS = _rwSqrt(rS);
    qpQuat->imag.x = 0.5f * rS;
    rS = 0.5f / rS;
    qpQuat->real = rS * (mpMatrix->up.z - mpMatrix->at.y);
    qpQuat->imag.y = rS * (mpMatrix->right.y + mpMatrix->up.x);
    qpQuat->imag.z = rS * (mpMatrix->right.z + mpMatrix->at.x);

    return qpQuat;
} // 0x802F7288

/*
 * ==================== QuatFromYDiagDomMatrix ====================
 * The matrix's trace is not positive and up.y is its largest diagonal element.
 * Name and local: Gormiti link map and DWARF (RtQuat.a rtquat.obj).
 */
static RtQuat* QuatFromYDiagDomMatrix(RtQuat* qpQuat, const RwMatrix* mpMatrix) {
    RwReal rS;

    rS = 1.0f + (mpMatrix->up.y - (mpMatrix->at.z + mpMatrix->right.x));
    rS = _rwSqrt(rS);
    qpQuat->imag.y = 0.5f * rS;
    rS = 0.5f / rS;
    qpQuat->real = rS * (mpMatrix->at.x - mpMatrix->right.z);
    qpQuat->imag.z = rS * (mpMatrix->up.z + mpMatrix->at.y);
    qpQuat->imag.x = rS * (mpMatrix->up.x + mpMatrix->right.y);

    return qpQuat;
} // 0x802F7330

/*
 * ==================== QuatFromZDiagDomMatrix ====================
 * The matrix's trace is not positive and at.z is its largest diagonal element.
 * Name and local: Gormiti link map and DWARF (RtQuat.a rtquat.obj).
 */
static RtQuat* QuatFromZDiagDomMatrix(RtQuat* qpQuat, const RwMatrix* mpMatrix) {
    RwReal rS;

    rS = 1.0f + (mpMatrix->at.z - (mpMatrix->right.x + mpMatrix->up.y));
    rS = _rwSqrt(rS);
    qpQuat->imag.z = 0.5f * rS;
    rS = 0.5f / rS;
    qpQuat->real = rS * (mpMatrix->right.y - mpMatrix->up.x);
    qpQuat->imag.x = rS * (mpMatrix->at.x + mpMatrix->right.z);
    qpQuat->imag.y = rS * (mpMatrix->at.y + mpMatrix->up.z);

    return qpQuat;
} // 0x802F73D8

/*
 * ==================== RtQuatConvertFromMatrix ====================
 * Sets qpQuat to the rotation of an orthonormal matrix. A positive trace gives the quaternion
 * directly; otherwise the form is chosen by the largest diagonal element.
 * Locals: Gormiti DWARF (RtQuat.a rtquat.obj).
 */
RwBool RtQuatConvertFromMatrix(RtQuat* qpQuat, const RwMatrix* mpMatrix) {
    RwBool result = FALSE;

    if (qpQuat != NULL && mpMatrix != NULL) {
        result = TRUE;
    }

    if (result) {
        RwReal rTR = mpMatrix->right.x + mpMatrix->up.y + mpMatrix->at.z;

        if (rTR > 0.0f) {
            QuatFromPositiveDiagMatrix(qpQuat, mpMatrix, rTR);
        } else {
            RtQuat* (*QuatFromDiagDomMatrixFunc)(RtQuat*, const RwMatrix*);

            if (mpMatrix->right.x > mpMatrix->up.y) {
                if (mpMatrix->right.x > mpMatrix->at.z) {
                    QuatFromDiagDomMatrixFunc = QuatFromXDiagDomMatrix;
                } else {
                    QuatFromDiagDomMatrixFunc = QuatFromZDiagDomMatrix;
                }
            } else {
                if (mpMatrix->up.y > mpMatrix->at.z) {
                    QuatFromDiagDomMatrixFunc = QuatFromYDiagDomMatrix;
                } else {
                    QuatFromDiagDomMatrixFunc = QuatFromZDiagDomMatrix;
                }
            }
            QuatFromDiagDomMatrixFunc(qpQuat, mpMatrix);
        }
    }

    return result;
} // 0x802F7480


/*
 * ==================== RtQuatRotate ====================
 * Rotates quat by angle degrees about axis: the rotation replaces the quaternion, or is
 * multiplied in before (PRECONCAT) or after (POSTCONCAT) it.
 */
RtQuat* RtQuatRotate(RtQuat* quat, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    RwReal halfAngle;
    RwReal sinHalf;

    if (quat != NULL && axis != NULL) {
        switch (combineOp) {
        case rwCOMBINEREPLACE:
            halfAngle = (RwReal)(3.14159265358979323846 / 360.0) * angle;
            sinHalf = (RwReal)sin(halfAngle);
            quat->real = (RwReal)cos(halfAngle);
            RwV3dNormalize(&quat->imag, axis);
            RwV3dScaleMacro(&quat->imag, &quat->imag, sinHalf);
            break;
        case rwCOMBINEPRECONCAT: {
            RtQuat compose;
            RtQuat source = *quat;

            halfAngle = (RwReal)(3.14159265358979323846 / 360.0) * angle;
            sinHalf = (RwReal)sin(halfAngle);
            compose.real = (RwReal)cos(halfAngle);
            RwV3dNormalize(&compose.imag, axis);
            RwV3dScaleMacro(&compose.imag, &compose.imag, sinHalf);
            RtQuatMultiply(quat, &source, &compose);
            break;
        }
        case rwCOMBINEPOSTCONCAT: {
            RtQuat compose;
            RtQuat source = *quat;

            halfAngle = (RwReal)(3.14159265358979323846 / 360.0) * angle;
            sinHalf = (RwReal)sin(halfAngle);
            compose.real = (RwReal)cos(halfAngle);
            RwV3dNormalize(&compose.imag, axis);
            RwV3dScaleMacro(&compose.imag, &compose.imag, sinHalf);
            RtQuatMultiply(quat, &compose, &source);
            break;
        }
        default:
            RWERROR((E_RW_BADPARAM, "Invalid combination type"));
            quat = NULL;
            break;
        }
    } else {
        RWERROR((E_RW_NULLP));
        quat = NULL;
    }

    return quat;
} // 0x802F75B4

/*
 * ==================== RtQuatTransformVectors ====================
 * Rotates numPoints vectors by the unit quaternion quat:
 * v' = (w*w - imag.imag) v + 2 (imag.v) imag + 2 w (imag x v).
 */
RwV3d* RtQuatTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, const RwInt32 numPoints,
                              const RtQuat* quat) {
    RwV3d* out = vectorsOut;
    const RwV3d* in = vectorsIn;
    RwV3d twoImag;
    RwV3d twoWImag;
    RwV3d cross;
    RwReal scale;
    RwReal dot;
    RwInt32 i;

    scale = quat->real * quat->real - RwV3dDotProductMacro(&quat->imag, &quat->imag);
    RwV3dAddMacro(&twoImag, &quat->imag, &quat->imag);
    RwV3dScaleMacro(&twoWImag, &twoImag, quat->real);

    for (i = 0; i < numPoints; i++) {
        dot = RwV3dDotProductMacro(&twoImag, in);
        RwV3dCrossProductMacro(&cross, &twoWImag, in);
        *out = cross;
        RwV3dIncrementScaledMacro(out, &quat->imag, dot);
        RwV3dIncrementScaledMacro(out, in, scale);
        in++;
        out++;
    }

    return vectorsOut;
} // 0x802F7994
