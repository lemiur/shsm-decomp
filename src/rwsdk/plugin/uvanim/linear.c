// Unit name and boundary: Gormiti link map (RpUVAnim.a linear.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpUVAnim.a linear.obj), Final and Debug builds.
/*
 * linear.c: RpUVAnim's linear keyframe scheme. A keyframe holds the six coefficients of a UV
 * transformation: a 2x2 matrix and a translation. The scheme's callbacks set a material's UV
 * matrix from an interpolated keyframe, interpolate and blend keyframes coefficient by
 * coefficient, multiply two transformations (add two keyframes), make a keyframe a delta from a
 * start frame and read a keyframe from a matrix.
 */
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plugin/uvanim/rpuvanim.h>

/*
 * ==================== RpUVAnimLinearKeyFrameApply ====================
 * Sets a UV matrix from an interpolated keyframe's coefficients: the 2x2 matrix in the matrix's
 * x and y axes, the translation in its position.
 */
void RpUVAnimLinearKeyFrameApply(void* pMatrix, void* pVoidIFrame) {
    RWASSERT(pMatrix);
    RWASSERT(pVoidIFrame);

    {
        RpUVAnimLinearKeyFrameData* lkf;
        RpUVAnimInterpFrame* iFrame;

        iFrame = (RpUVAnimInterpFrame*)pVoidIFrame;
        lkf = &iFrame->data.linear;

        ((RwMatrix*)pMatrix)->right.x = lkf->uv[0];
        ((RwMatrix*)pMatrix)->right.y = lkf->uv[1];
        ((RwMatrix*)pMatrix)->right.z = 0.0f;
        ((RwMatrix*)pMatrix)->up.x = lkf->uv[2];
        ((RwMatrix*)pMatrix)->up.y = lkf->uv[3];
        ((RwMatrix*)pMatrix)->up.z = 0.0f;
        ((RwMatrix*)pMatrix)->at.x = 0.0f;
        ((RwMatrix*)pMatrix)->at.y = 0.0f;
        ((RwMatrix*)pMatrix)->at.z = 0.0f;
        ((RwMatrix*)pMatrix)->pos.x = lkf->uv[4];
        ((RwMatrix*)pMatrix)->pos.y = lkf->uv[5];
        ((RwMatrix*)pMatrix)->pos.z = 0.0f;
    }

    RwMatrixUpdate((RwMatrix*)pMatrix);
} // 0x802CC0E4

/*
 * ==================== RpUVAnimLinearKeyFrameInterpolate ====================
 * Interpolates each coefficient between two keyframes at a time.
 */
void RpUVAnimLinearKeyFrameInterpolate(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal time,
                                       void* customData) {
    RpUVAnimInterpFrame* pOut;
    RpUVAnimKeyFrame* pIn1;
    RpUVAnimKeyFrame* pIn2;
    RpUVAnimLinearKeyFrameData* kOut;
    RpUVAnimLinearKeyFrameData* kIn1;
    RpUVAnimLinearKeyFrameData* kIn2;
    RwReal fAlpha;

    RWASSERT(pVoidOut);
    RWASSERT(pVoidIn1);
    RWASSERT(pVoidIn2);

    pOut = (RpUVAnimInterpFrame*)pVoidOut;
    pIn1 = (RpUVAnimKeyFrame*)pVoidIn1;
    pIn2 = (RpUVAnimKeyFrame*)pVoidIn2;

    kOut = &pOut->data.linear;
    kIn1 = &pIn1->data.linear;
    kIn2 = &pIn2->data.linear;

    fAlpha = (time - pIn1->time) / (pIn2->time - pIn1->time);

    kOut->uv[0] = kIn1->uv[0] + fAlpha * (kIn2->uv[0] - kIn1->uv[0]);
    kOut->uv[1] = kIn1->uv[1] + fAlpha * (kIn2->uv[1] - kIn1->uv[1]);
    kOut->uv[2] = kIn1->uv[2] + fAlpha * (kIn2->uv[2] - kIn1->uv[2]);
    kOut->uv[3] = kIn1->uv[3] + fAlpha * (kIn2->uv[3] - kIn1->uv[3]);
    kOut->uv[4] = kIn1->uv[4] + fAlpha * (kIn2->uv[4] - kIn1->uv[4]);
    kOut->uv[5] = kIn1->uv[5] + fAlpha * (kIn2->uv[5] - kIn1->uv[5]);
} // 0x802CC134

/*
 * ==================== RpUVAnimLinearKeyFrameBlend ====================
 * Blends two interpolated keyframes coefficient by coefficient: out = in1 + alpha * (in2 - in1).
 */
void RpUVAnimLinearKeyFrameBlend(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal fAlpha) {
    RpUVAnimInterpFrame* pOut;
    RpUVAnimInterpFrame* pIn1;
    RpUVAnimInterpFrame* pIn2;
    RpUVAnimLinearKeyFrameData* kOut;
    RpUVAnimLinearKeyFrameData* kIn1;
    RpUVAnimLinearKeyFrameData* kIn2;

    RWASSERT(pVoidOut);
    RWASSERT(pVoidIn1);
    RWASSERT(pVoidIn2);

    pOut = (RpUVAnimInterpFrame*)pVoidOut;
    pIn1 = (RpUVAnimInterpFrame*)pVoidIn1;
    pIn2 = (RpUVAnimInterpFrame*)pVoidIn2;

    kOut = &pOut->data.linear;
    kIn1 = &pIn1->data.linear;
    kIn2 = &pIn2->data.linear;

    kOut->uv[0] = kIn1->uv[0] + fAlpha * (kIn2->uv[0] - kIn1->uv[0]);
    kOut->uv[1] = kIn1->uv[1] + fAlpha * (kIn2->uv[1] - kIn1->uv[1]);
    kOut->uv[2] = kIn1->uv[2] + fAlpha * (kIn2->uv[2] - kIn1->uv[2]);
    kOut->uv[3] = kIn1->uv[3] + fAlpha * (kIn2->uv[3] - kIn1->uv[3]);
    kOut->uv[4] = kIn1->uv[4] + fAlpha * (kIn2->uv[4] - kIn1->uv[4]);
    kOut->uv[5] = kIn1->uv[5] + fAlpha * (kIn2->uv[5] - kIn1->uv[5]);
} // 0x802CC1DC

/*
 * ==================== RpUVAnimLinearKeyFrameMulRecip ====================
 * Makes a keyframe a delta from a start frame: its 2x2 matrix is multiplied by the inverse of
 * the start's (when the start has one) and the start's translation is subtracted.
 * The original's third and fourth products use the third inverse coefficient twice where the
 * fourth belongs (the matrix product's second column), so the result is wrong for a start whose
 * matrix is not a pure scale; the bytes keep that.
 */
void RpUVAnimLinearKeyFrameMulRecip(void* pVoidFrame, void* pVoidStart) {
    RpUVAnimKeyFrame* pAnimFrame;
    RpUVAnimKeyFrame* pStartFrame;
    RpUVAnimLinearKeyFrameData* kStart;
    RpUVAnimLinearKeyFrameData* kNew;
    RwReal det;
    RwReal* uvStart;
    RwReal* uvNew;

    RWASSERT(pVoidFrame);
    RWASSERT(pVoidStart);

    pAnimFrame = (RpUVAnimKeyFrame*)pVoidFrame;
    pStartFrame = (RpUVAnimKeyFrame*)pVoidStart;

    kStart = &pStartFrame->data.linear;
    kNew = &pAnimFrame->data.linear;

    uvStart = kStart->uv;
    uvNew = kNew->uv;

    det = uvStart[0] * uvStart[3] - uvStart[1] * uvStart[2];

    if (det) {
        RwReal invDet;
        RwReal uvInv[4];
        RwReal uvOrig[4];

        invDet = 1.0f / det;

        uvOrig[0] = uvNew[0];
        uvOrig[1] = uvNew[1];
        uvOrig[2] = uvNew[2];
        uvOrig[3] = uvNew[3];

        uvInv[0] = uvStart[3] * invDet;
        uvInv[1] = -uvStart[1] * invDet;
        uvInv[2] = -uvStart[2] * invDet;
        uvInv[3] = uvStart[0] * invDet;

        uvNew[0] = uvInv[0] * uvOrig[0] + uvInv[1] * uvOrig[2];
        uvNew[1] = uvInv[0] * uvOrig[1] + uvInv[1] * uvOrig[3];
        uvNew[2] = uvInv[2] * uvOrig[0] + uvInv[2] * uvOrig[2];
        uvNew[3] = uvInv[2] * uvOrig[1] + uvInv[2] * uvOrig[3];
    }

    uvNew[4] -= uvStart[4];
    uvNew[5] -= uvStart[5];
} // 0x802CC270

/*
 * ==================== RpUVAnimLinearKeyFrameAdd ====================
 * Concatenates two interpolated keyframes: the product of their 2x2 matrices and the sum of
 * their translations.
 */
void RpUVAnimLinearKeyFrameAdd(void* pVoidOut, void* pVoidIn1, void* pVoidIn2) {
    RpUVAnimInterpFrame* pOut;
    RpUVAnimInterpFrame* pIn1;
    RpUVAnimInterpFrame* pIn2;
    RpUVAnimLinearKeyFrameData* kOut;
    RpUVAnimLinearKeyFrameData* kIn1;
    RpUVAnimLinearKeyFrameData* kIn2;

    RWASSERT(pVoidOut);
    RWASSERT(pVoidIn1);
    RWASSERT(pVoidIn2);

    pOut = (RpUVAnimInterpFrame*)pVoidOut;
    pIn1 = (RpUVAnimInterpFrame*)pVoidIn1;
    pIn2 = (RpUVAnimInterpFrame*)pVoidIn2;

    kOut = &pOut->data.linear;
    kIn1 = &pIn1->data.linear;
    kIn2 = &pIn2->data.linear;

    kOut->uv[0] = kIn1->uv[0] * kIn2->uv[0] + kIn1->uv[1] * kIn2->uv[2];
    kOut->uv[1] = kIn1->uv[0] * kIn2->uv[1] + kIn1->uv[1] * kIn2->uv[3];
    kOut->uv[2] = kIn1->uv[2] * kIn2->uv[0] + kIn1->uv[3] * kIn2->uv[2];
    kOut->uv[3] = kIn1->uv[2] * kIn2->uv[1] + kIn1->uv[3] * kIn2->uv[3];

    {
        RwReal* uvA = kIn1->uv;
        RwReal* uvB = kIn2->uv;

        kOut->uv[4] = uvA[4] + uvB[4];
        kOut->uv[5] = uvA[5] + uvB[5];
    }
} // 0x802CC354

/*
 * ==================== RpUVAnimLinearKeyFrameDataInitFromMatrix ====================
 * Sets keyframe data from a UV matrix: its x and y axes and position. Returns the data.
 */
RpUVAnimLinearKeyFrameData* RpUVAnimLinearKeyFrameDataInitFromMatrix(RpUVAnimLinearKeyFrameData* data,
                                                                     const RwMatrix* matrix) {
    RWASSERT(data);
    RWASSERT(matrix);

    data->uv[0] = matrix->right.x;
    data->uv[1] = matrix->right.y;
    data->uv[2] = matrix->up.x;
    data->uv[3] = matrix->up.y;
    data->uv[4] = matrix->pos.x;
    data->uv[5] = matrix->pos.y;

    return data;
} // 0x802CC3F8
