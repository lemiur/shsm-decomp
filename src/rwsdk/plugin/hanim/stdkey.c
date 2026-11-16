// Unit name and boundary: Gormiti link map (RpHAnim.a stdkey.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpHAnim.a stdkey.obj).
/*
 * stdkey.c: RpHAnim's standard keyframe scheme. A keyframe is a time, a rotation quaternion and
 * a translation; the scheme's callbacks apply an interpolated keyframe to a matrix, interpolate
 * and blend by spherical linear interpolation of the rotations, stream the keyframes, make an
 * animation a delta from a start frame and add two interpolated keyframes.
 */
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/plcore/rwfdlibm.h>
#include <rwsdk/plugin/hanim/rphanim.h>

/*
 * ==================== RpHAnimKeyFrameApply ====================
 * Sets a matrix to an interpolated keyframe's rotation and translation.
 */
void RpHAnimKeyFrameApply(void* pMatrix, void* pVoidIFrame) {
    RpHAnimInterpFrame* iFrame;

    iFrame = (RpHAnimInterpFrame*)pVoidIFrame;
    RtQuatUnitConvertToMatrix(&iFrame->q, (RwMatrix*)pMatrix);
    ((RwMatrix*)pMatrix)->pos.x = iFrame->t.x;
    ((RwMatrix*)pMatrix)->pos.y = iFrame->t.y;
    ((RwMatrix*)pMatrix)->pos.z = iFrame->t.z;
} // 0x802C5DD8

/*
 * ==================== RpHAnimKeyFrameInterpolate ====================
 * Interpolates the translation linearly and the rotation spherically between two keyframes. The
 * second rotation is negated in place when the two are more than a right angle apart, so the
 * path is the short one; rotations closer than the cosine 0.999 are blended linearly.
 */
void RpHAnimKeyFrameInterpolate(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal time, void* customData) {
    RpHAnimInterpFrame* pOut;
    RpHAnimKeyFrame* pIn1;
    RpHAnimKeyFrame* pIn2;
    RwReal fCosTheta;
    RwReal fAlpha;
    RwReal fBeta;
    RwBool bObtuseTheta;
    RwBool bNearlyZeroTheta;

    pOut = (RpHAnimInterpFrame*)pVoidOut;
    pIn1 = (RpHAnimKeyFrame*)pVoidIn1;
    pIn2 = (RpHAnimKeyFrame*)pVoidIn2;

    fCosTheta = RwV3dDotProductMacro(&pIn1->q.imag, &pIn2->q.imag) + pIn1->q.real * pIn2->q.real;

    fAlpha = (time - pIn1->time) / (pIn2->time - pIn1->time);

    pOut->t.x = pIn1->t.x + fAlpha * (pIn2->t.x - pIn1->t.x);
    pOut->t.y = pIn1->t.y + fAlpha * (pIn2->t.y - pIn1->t.y);
    pOut->t.z = pIn1->t.z + fAlpha * (pIn2->t.z - pIn1->t.z);

    bObtuseTheta = (fCosTheta < 0.0f);
    if (bObtuseTheta) {
        fCosTheta = -fCosTheta;
        RwV3dNegateMacro(&pIn2->q.imag, &pIn2->q.imag);
        pIn2->q.real = -pIn2->q.real;
    }

    fBeta = 1.0f - fAlpha;
    bNearlyZeroTheta = (fCosTheta >= 0.999f);
    if (!bNearlyZeroTheta) {
        RwReal fTheta;
        RwReal fCosecTheta;

        rwACosFMacro(fTheta, fCosTheta);
        rwKernelSinFMacro(fCosecTheta, fTheta);
        fCosecTheta = 1.0f / fCosecTheta;
        fBeta *= fTheta;
        rwKernelSinFMacro(fBeta, fBeta);
        fBeta *= fCosecTheta;

        fAlpha *= fTheta;
        rwKernelSinFMacro(fAlpha, fAlpha);
        fAlpha *= fCosecTheta;
    }

    pOut->q.imag.x = fBeta * pIn1->q.imag.x + fAlpha * pIn2->q.imag.x;
    pOut->q.imag.y = fBeta * pIn1->q.imag.y + fAlpha * pIn2->q.imag.y;
    pOut->q.imag.z = fBeta * pIn1->q.imag.z + fAlpha * pIn2->q.imag.z;
    pOut->q.real = fBeta * pIn1->q.real + fAlpha * pIn2->q.real;
} // 0x802C5F4C

/*
 * ==================== RpHAnimKeyFrameBlend ====================
 * Blends two interpolated keyframes by alpha: the same linear and spherical interpolation as
 * RpHAnimKeyFrameInterpolate, with alpha given.
 */
void RpHAnimKeyFrameBlend(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal fAlpha) {
    RpHAnimInterpFrame* pOut;
    RpHAnimInterpFrame* pIn1;
    RpHAnimInterpFrame* pIn2;
    RwReal fBeta, fTheta;
    RwReal fCosTheta;
    RwBool bObtuseTheta;
    RwBool bNearlyZeroTheta;

    pOut = (RpHAnimInterpFrame*)pVoidOut;
    pIn1 = (RpHAnimInterpFrame*)pVoidIn1;
    pIn2 = (RpHAnimInterpFrame*)pVoidIn2;

    fCosTheta = RwV3dDotProductMacro(&pIn1->q.imag, &pIn2->q.imag) + pIn1->q.real * pIn2->q.real;

    pOut->t.x = pIn1->t.x + fAlpha * (pIn2->t.x - pIn1->t.x);
    pOut->t.y = pIn1->t.y + fAlpha * (pIn2->t.y - pIn1->t.y);
    pOut->t.z = pIn1->t.z + fAlpha * (pIn2->t.z - pIn1->t.z);

    bObtuseTheta = (fCosTheta < 0.0f);
    if (bObtuseTheta) {
        fCosTheta = -fCosTheta;
        RwV3dNegateMacro(&pIn2->q.imag, &pIn2->q.imag);
        pIn2->q.real = -pIn2->q.real;
    }

    fBeta = 1.0f - fAlpha;
    bNearlyZeroTheta = (fCosTheta >= 0.999f);
    if (!bNearlyZeroTheta) {
        RwReal fCosecTheta;

        rwACosFMacro(fTheta, fCosTheta);
        rwKernelSinFMacro(fCosecTheta, fTheta);
        fCosecTheta = 1.0f / fCosecTheta;
        fBeta *= fTheta;
        rwKernelSinFMacro(fBeta, fBeta);
        fBeta *= fCosecTheta;

        fAlpha *= fTheta;
        rwKernelSinFMacro(fAlpha, fAlpha);
        fAlpha *= fCosecTheta;
    }

    pOut->q.imag.x = fBeta * pIn1->q.imag.x + fAlpha * pIn2->q.imag.x;
    pOut->q.imag.y = fBeta * pIn1->q.imag.y + fAlpha * pIn2->q.imag.y;
    pOut->q.imag.z = fBeta * pIn1->q.imag.z + fAlpha * pIn2->q.imag.z;
    pOut->q.real = fBeta * pIn1->q.real + fAlpha * pIn2->q.real;
} // 0x802C6468

/*
 * ==================== RpHAnimKeyFrameStreamRead ====================
 * Reads an animation's keyframes: for each, its time, rotation and translation, then the offset
 * in bytes of its previous keyframe in the array.
 */
RtAnimAnimation* RpHAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* pAnimation) {
    RwInt32 i;
    RwInt32 temp;
    RpHAnimKeyFrame* pFrames;

    pFrames = (RpHAnimKeyFrame*)pAnimation->pFrames;
    for (i = 0; i < pAnimation->numFrames; i++) {
        if (!RwStreamReadReal(stream, &pFrames[i].time, sizeof(RpHAnimKeyFrame) - sizeof(RpHAnimKeyFrame*))) {
            return NULL;
        }
        if (!RwStreamReadInt32(stream, &temp, sizeof(RwInt32))) {
            return NULL;
        }
        pFrames[i].prevFrame = &pFrames[temp / sizeof(RpHAnimKeyFrame)];
    }

    return pAnimation;
} // 0x802C6974

/*
 * ==================== RpHAnimKeyFrameStreamWrite ====================
 * Writes an animation's keyframes in the form RpHAnimKeyFrameStreamRead reads.
 */
RwBool RpHAnimKeyFrameStreamWrite(const RtAnimAnimation* pAnimation, RwStream* stream) {
    RwInt32 i;
    RwInt32 temp;
    RpHAnimKeyFrame* pFrames;

    pFrames = (RpHAnimKeyFrame*)pAnimation->pFrames;
    for (i = 0; i < pAnimation->numFrames; i++) {
        if (!RwStreamWriteReal(stream, &pFrames[i].time, sizeof(RpHAnimKeyFrame) - sizeof(RpHAnimKeyFrame*))) {
            return FALSE;
        }
        temp = (RwUInt8*)pFrames[i].prevFrame - (RwUInt8*)pFrames;
        if (!RwStreamWriteInt32(stream, &temp, sizeof(RwInt32))) {
            return FALSE;
        }
    }

    return TRUE;
} // 0x802C6A28

/*
 * ==================== RpHAnimKeyFrameStreamGetSize ====================
 * The bytes the keyframes of an animation take in a stream.
 */
RwInt32 RpHAnimKeyFrameStreamGetSize(const RtAnimAnimation* pAnimation) {
    RwInt32 size;

    size = sizeof(RpHAnimKeyFrame);
    size *= pAnimation->numFrames;
    return size;
} // 0x802C6AC8

/*
 * ==================== RpHAnimKeyFrameMulRecip ====================
 * Makes a keyframe relative to a start frame: its rotation is multiplied by the start frame's
 * reciprocal and the start frame's translation is taken from its translation.
 */
void RpHAnimKeyFrameMulRecip(void* pVoidFrame, void* pVoidStart) {
    RpHAnimKeyFrame* pAnimFrame;
    RpHAnimKeyFrame* pStartFrame;
    RtQuat qInverse, qFrame;

    pAnimFrame = (RpHAnimKeyFrame*)pVoidFrame;
    pStartFrame = (RpHAnimKeyFrame*)pVoidStart;

    RtQuatReciprocal(&qInverse, &pStartFrame->q);
    qFrame = pAnimFrame->q;
    RtQuatMultiply(&pAnimFrame->q, &qInverse, &qFrame);

    pAnimFrame->t.x -= pStartFrame->t.x;
    pAnimFrame->t.y -= pStartFrame->t.y;
    pAnimFrame->t.z -= pStartFrame->t.z;
} // 0x802C6AD8

/*
 * ==================== RpHAnimKeyFrameAdd ====================
 * Adds two interpolated keyframes: multiplies their rotations and adds their translations.
 */
void RpHAnimKeyFrameAdd(void* pVoidOut, void* pVoidIn1, void* pVoidIn2) {
    RpHAnimInterpFrame* pOut;
    RpHAnimInterpFrame* pIn1;
    RpHAnimInterpFrame* pIn2;

    pOut = (RpHAnimInterpFrame*)pVoidOut;
    pIn1 = (RpHAnimInterpFrame*)pVoidIn1;
    pIn2 = (RpHAnimInterpFrame*)pVoidIn2;

    RtQuatMultiply(&pOut->q, &pIn1->q, &pIn2->q);

    pOut->t.x = pIn1->t.x + pIn2->t.x;
    pOut->t.y = pIn1->t.y + pIn2->t.y;
    pOut->t.z = pIn1->t.z + pIn2->t.z;
} // 0x802C6C98
