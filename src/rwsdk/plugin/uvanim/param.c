// Unit name and boundary: Gormiti link map (RpUVAnim.a param.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpUVAnim.a param.obj), Final and Debug builds.
/*
 * param.c: RpUVAnim's parametric keyframe scheme. A keyframe holds a rotation, two scales, a skew
 * and a translation, which the scheme turns into a UV matrix that rotates and scales about the
 * centre of the texture. Rotations interpolate by the short way round; the multiply-by-reciprocal
 * and add callbacks of the scheme do nothing.
 */
#include <math.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/plugin/uvanim/rpuvanim.h>

/*
 * ==================== RpUVAnimParamKeyFrameApply ====================
 * Sets a UV matrix from an interpolated keyframe: the scales and skew in its x and y axes, the
 * translation in its position, then the rotation about the pivot.
 */
void RpUVAnimParamKeyFrameApply(void* pMatrix, void* pVoidIFrame) {
    RWASSERT(pMatrix);
    RWASSERT(pVoidIFrame);

    {
        RwV3d zAxis = {0.0f, 0.0f, 1.0f};
        RpUVAnimParamKeyFrameData* param;

        param = &((RpUVAnimInterpFrame*)pVoidIFrame)->data.param;

        ((RwMatrix*)pMatrix)->right.x = param->s0;
        ((RwMatrix*)pMatrix)->right.y = param->skew;
        ((RwMatrix*)pMatrix)->right.z = 0.0f;
        ((RwMatrix*)pMatrix)->up.x = 0.0f;
        ((RwMatrix*)pMatrix)->up.y = param->s1;
        ((RwMatrix*)pMatrix)->up.z = 0.0f;
        ((RwMatrix*)pMatrix)->at.x = 0.0f;
        ((RwMatrix*)pMatrix)->at.y = 0.0f;
        ((RwMatrix*)pMatrix)->at.z = 0.0f;
        ((RwMatrix*)pMatrix)->pos.x = param->x;
        ((RwMatrix*)pMatrix)->pos.y = param->y;
        ((RwMatrix*)pMatrix)->pos.z = 0.0f;

        RwMatrixUpdate((RwMatrix*)pMatrix);
        RwMatrixTranslate((RwMatrix*)pMatrix, &rpUVAnimPivot, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate((RwMatrix*)pMatrix, &zAxis, 180.0f * (param->theta / rwPI), rwCOMBINEPOSTCONCAT);
        RwMatrixTranslate((RwMatrix*)pMatrix, &rpUVAnimInvPivot, rwCOMBINEPOSTCONCAT);
    }
} // 0x802CC42C

/*
 * ==================== RpUVAnimParamKeyFrameInterpolate ====================
 * Interpolates each parameter between two keyframes at a time; the rotation takes the shorter
 * way round the circle.
 */
void RpUVAnimParamKeyFrameInterpolate(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal time,
                                      void* customData) {
    RpUVAnimInterpFrame* pOut;
    RpUVAnimKeyFrame* pIn1;
    RpUVAnimKeyFrame* pIn2;
    RpUVAnimParamKeyFrameData* kOut;
    RpUVAnimParamKeyFrameData* kIn1;
    RpUVAnimParamKeyFrameData* kIn2;
    RwReal fAlpha;

    RWASSERT(pVoidOut);
    RWASSERT(pVoidIn1);
    RWASSERT(pVoidIn2);

    pOut = (RpUVAnimInterpFrame*)pVoidOut;
    pIn1 = (RpUVAnimKeyFrame*)pVoidIn1;
    pIn2 = (RpUVAnimKeyFrame*)pVoidIn2;

    kOut = &pOut->data.param;
    kIn1 = &pIn1->data.param;
    kIn2 = &pIn2->data.param;

    fAlpha = (time - pIn1->time) / (pIn2->time - pIn1->time);

    {
        RwReal deltaTheta = kIn2->theta - kIn1->theta;

        if (deltaTheta < -rwPI) {
            deltaTheta += 2.0f * rwPI;
        } else if (deltaTheta > rwPI) {
            deltaTheta -= 2.0f * rwPI;
        }

        kOut->theta = kIn1->theta + fAlpha * deltaTheta;
    }

    kOut->s0 = kIn1->s0 + fAlpha * (kIn2->s0 - kIn1->s0);
    kOut->s1 = kIn1->s1 + fAlpha * (kIn2->s1 - kIn1->s1);
    kOut->skew = kIn1->skew + fAlpha * (kIn2->skew - kIn1->skew);
    kOut->x = kIn1->x + fAlpha * (kIn2->x - kIn1->x);
    kOut->y = kIn1->y + fAlpha * (kIn2->y - kIn1->y);
} // 0x802CC514

/*
 * ==================== RpUVAnimParamKeyFrameBlend ====================
 * Blends two interpolated keyframes parameter by parameter: out = in1 + alpha * (in2 - in1), the
 * rotation by the shorter way round.
 */
void RpUVAnimParamKeyFrameBlend(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal fAlpha) {
    RpUVAnimInterpFrame* pOut;
    RpUVAnimInterpFrame* pIn1;
    RpUVAnimInterpFrame* pIn2;
    RpUVAnimParamKeyFrameData* kOut;
    RpUVAnimParamKeyFrameData* kIn1;
    RpUVAnimParamKeyFrameData* kIn2;

    RWASSERT(pVoidOut);
    RWASSERT(pVoidIn1);
    RWASSERT(pVoidIn2);

    pOut = (RpUVAnimInterpFrame*)pVoidOut;
    pIn1 = (RpUVAnimInterpFrame*)pVoidIn1;
    pIn2 = (RpUVAnimInterpFrame*)pVoidIn2;

    kOut = &pOut->data.param;
    kIn1 = &pIn1->data.param;
    kIn2 = &pIn2->data.param;

    {
        RwReal deltaTheta = kIn2->theta - kIn1->theta;

        if (deltaTheta < -rwPI) {
            deltaTheta += 2.0f * rwPI;
        } else if (deltaTheta > rwPI) {
            deltaTheta -= 2.0f * rwPI;
        }

        kOut->theta = kIn1->theta + fAlpha * deltaTheta;
    }

    kOut->s0 = kIn1->s0 + fAlpha * (kIn2->s0 - kIn1->s0);
    kOut->s1 = kIn1->s1 + fAlpha * (kIn2->s1 - kIn1->s1);
    kOut->skew = kIn1->skew + fAlpha * (kIn2->skew - kIn1->skew);
    kOut->x = kIn1->x + fAlpha * (kIn2->x - kIn1->x);
    kOut->y = kIn1->y + fAlpha * (kIn2->y - kIn1->y);
} // 0x802CC5EC

/*
 * ==================== RpUVAnimParamKeyFrameMulRecip ====================
 * The parametric scheme makes no delta keyframes: nothing to do in a release build.
 */
void RpUVAnimParamKeyFrameMulRecip(void* pVoidFrame, void* pVoidStart) {
    RWASSERT(pVoidFrame);
    RWASSERT(pVoidStart);
} // 0x802CC6B0

/*
 * ==================== RpUVAnimParamKeyFrameAdd ====================
 * The parametric scheme adds no keyframes: nothing to do in a release build.
 */
void RpUVAnimParamKeyFrameAdd(void* pVoidOut, void* pVoidIn1, void* pVoidIn2) {
    RWASSERT(pVoidOut);
    RWASSERT(pVoidIn1);
    RWASSERT(pVoidIn2);
} // 0x802CC6B4

/*
 * ==================== RpUVAnimParamKeyFrameDataInitFromMatrix ====================
 * Sets keyframe data from a UV matrix by undoing what RpUVAnimParamKeyFrameApply does: the
 * rotation is read from the matrix's x axis, the pivot translations and the rotation are taken
 * out of a copy, and the scales, skew and translation are read from what is left. Returns the
 * data.
 */
RpUVAnimParamKeyFrameData* RpUVAnimParamKeyFrameDataInitFromMatrix(RpUVAnimParamKeyFrameData* data,
                                                                   const RwMatrix* matrix) {
    RwMatrix m;

    RWASSERT(data);
    RWASSERT(matrix);

    m = *matrix;

    {
        RwV3d ZAxis = {0.0f, 0.0f, 1.0f};
        RwReal a;
        RwReal c;

        a = m.right.x;
        c = m.up.x;

        data->theta = (RwReal)atan2(-c, a);

        RwMatrixTranslate(&m, &rpUVAnimPivot, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(&m, &ZAxis, 180.0f * (-data->theta / rwPI), rwCOMBINEPOSTCONCAT);
        RwMatrixTranslate(&m, &rpUVAnimInvPivot, rwCOMBINEPOSTCONCAT);

        data->s0 = m.right.x;
        data->skew = m.right.y;
        data->s1 = m.up.y;
        data->x = m.pos.x;
        data->y = m.pos.y;
    }

    return data;
} // 0x802CC6B8
