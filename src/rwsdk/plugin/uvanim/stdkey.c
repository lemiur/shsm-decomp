// Unit name and boundary: Gormiti link map (RpUVAnim.a stdkey.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpUVAnim.a stdkey.obj), Final and Debug builds.
/*
 * stdkey.c: RpUVAnim's keyframe functions shared by its two schemes: making a keyframe from a
 * matrix and streaming an animation's keyframes (each keyframe's time and data, and the index of
 * the keyframe it replaces) together with the animation's custom data.
 */
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plugin/uvanim/rpuvanim.h>

/*
 * ==================== RpUVAnimKeyFrameInit ====================
 * Sets a keyframe's previous frame, time and data from a matrix, in the scheme the animation
 * uses. It returns NULL whatever happens, as the original does.
 */
RpUVAnimKeyFrame* RpUVAnimKeyFrameInit(const RtAnimAnimation* animation, RpUVAnimKeyFrame* keyFrame,
                                       RpUVAnimKeyFrame* prevFrame, RwReal time, RwMatrix* matrix) {
    RWASSERT(animation);
    RWASSERT(keyFrame);
    RWASSERT(prevFrame);
    RWASSERT(matrix);

    keyFrame->prevFrame = prevFrame;
    keyFrame->time = time;

    if (RtAnimAnimationGetTypeID(animation) == _rpUVAnimLinearInterpolatorInfo.typeID) {
        if (RpUVAnimLinearKeyFrameDataInitFromMatrix(&keyFrame->data.linear, matrix)) {
            return NULL;
        }
    }

    if (RtAnimAnimationGetTypeID(animation) == _rpUVAnimParamInterpolatorInfo.typeID) {
        if (RpUVAnimParamKeyFrameDataInitFromMatrix(&keyFrame->data.param, matrix)) {
            return NULL;
        }
    }

    return NULL;
} // 0x802CD9F4

/*
 * ==================== RpUVAnimKeyFrameStreamRead ====================
 * Reads an animation's custom data and keyframes. A keyframe's previous frame is streamed as its
 * index.
 */
RtAnimAnimation* RpUVAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* pAnimation) {
    RwUInt32 version;
    RwInt32 i;
    RwInt32 temp;
    RpUVAnimKeyFrame* pFrames;
    _rpUVAnimCustomData* customData;

    RWASSERT(stream);
    RWASSERT(pAnimation);

    if (!RwStreamReadInt32(stream, (RwInt32*)&version, sizeof(version))) {
        return NULL;
    }

    customData = _rpUVAnimCustomDataStreamRead(stream);
    if (customData == NULL) {
        return NULL;
    }

    pFrames = (RpUVAnimKeyFrame*)pAnimation->pFrames;
    pAnimation->customData = customData;

    for (i = 0; i < pAnimation->numFrames; i++) {
        if (!RwStreamReadReal(stream, &pFrames[i].time, sizeof(RwReal) + sizeof(RpUVAnimKeyFrameData))) {
            return NULL;
        }

        if (!RwStreamReadInt32(stream, &temp, sizeof(temp))) {
            return NULL;
        }

        pFrames[i].prevFrame = &pFrames[temp];
    }

    return pAnimation;
} // 0x802CDAA0

/*
 * ==================== RpUVAnimKeyFrameStreamWrite ====================
 * Writes what RpUVAnimKeyFrameStreamRead reads.
 */
RwBool RpUVAnimKeyFrameStreamWrite(const RtAnimAnimation* pAnimation, RwStream* stream) {
    RwUInt32 version = 0;
    RwInt32 i;
    RwInt32 temp;
    RpUVAnimKeyFrame* pFrames;

    RWASSERT(pAnimation);
    RWASSERT(stream);

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&version, sizeof(version))) {
        return FALSE;
    }

    if (!_rpUVAnimCustomDataStreamWrite((const _rpUVAnimCustomData*)pAnimation->customData, stream)) {
        return FALSE;
    }

    pFrames = (RpUVAnimKeyFrame*)pAnimation->pFrames;

    for (i = 0; i < pAnimation->numFrames; i++) {
        if (!RwStreamWriteReal(stream, &pFrames[i].time, sizeof(RwReal) + sizeof(RpUVAnimKeyFrameData))) {
            return FALSE;
        }

        temp = pFrames[i].prevFrame - pFrames;
        if (!RwStreamWriteInt32(stream, &temp, sizeof(temp))) {
            return FALSE;
        }
    }

    return TRUE;
} // 0x802CDB7C

/*
 * ==================== RpUVAnimKeyFrameStreamGetSize ====================
 * The bytes RpUVAnimKeyFrameStreamWrite writes: the version, the custom data and, per keyframe,
 * its time, data and previous frame index.
 */
RwInt32 RpUVAnimKeyFrameStreamGetSize(const RtAnimAnimation* pAnimation) {
    RwInt32 size;

    RWASSERT(pAnimation);

    size = sizeof(RwUInt32);
    size += _rpUVAnimCustomDataStreamGetSize((const _rpUVAnimCustomData*)pAnimation->customData);
    size += pAnimation->numFrames * (sizeof(RwReal) + sizeof(RpUVAnimKeyFrameData) + sizeof(RwInt32));

    return size;
} // 0x802CDC68
