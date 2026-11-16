// Reference: tgsm/pbg config/GPLE9G/symbols.txt (function names, unit layout)
/*
 * rtanim.c: the keyframe animation toolkit. It keeps the table of interpolation schemes the
 * animation plugins register, creates, streams and destroys animations, and plays them through
 * interpolators: each keeps, per node, the pair of keyframes the current time lies between and
 * the keyframe interpolated from them. RtAnimAnimationFreeListCreateParams, RtAnimAnimationRead,
 * RtAnimAnimationWrite, RtAnimInterpolatorSetAnimCallBack, RtAnimAnimationMakeDelta and
 * RtAnimInterpolatorAddSubInterpolator have no copy in retail (dead-stripped).
 */
#include <rwsdk/badevice.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/tool/anim/rtanim.h>
#include <string.h>

#define rwPLUGIN_ID rwID_ANIMTOOLKIT
#include <rwsdk/plcore/rpdbgerr.h>

/*
 * The RtAnim library linked here was built from an SDK build whose RWBUILDNUMBER was 0x55 (the
 * core's is 0x65): the chunk headers it writes carry that number.
 */
#undef RWBUILDNUMBER
#define RWBUILDNUMBER 0x55

/* The animation free list's block size and preallocated blocks. Names invented. */
static RwInt32 _rtAnimAnimationFreeListBlockSize = 128;
static RwInt32 _rtAnimAnimationFreeListPreallocBlocks = 1;

/* The animation free list (created at open, unused otherwise) and its space. Names invented. */
static RwFreeList _rtAnimAnimationFreeListSpace;
static RwFreeList* _rtAnimAnimationFreeList;

RtAnimInterpolatorInfo RtAnimInterpolatorInfoBlock[rtANIMMAXINTERPOLATIONSCHEMES];
RwInt32 RtAnimInterpolatorInfoBlockNumEntries;

/*
 * ==================== AnimOpen ====================
 * Creates the animation free list when an engine instance opens.
 */
static void* AnimOpen(void* instance, RwInt32 offset, RwInt32 size) {
    _rtAnimAnimationFreeList = RwFreeListCreateAndPreallocateSpace(
        sizeof(RtAnimAnimation), _rtAnimAnimationFreeListBlockSize, sizeof(RwInt32),
        _rtAnimAnimationFreeListPreallocBlocks, &_rtAnimAnimationFreeListSpace,
        rwMEMHINTDUR_GLOBAL | rwID_HANIMANIMATION);
    if (_rtAnimAnimationFreeList == NULL) {
        instance = NULL;
    }
    return instance;
} // 0x802F5F10

/*
 * ==================== AnimClose ====================
 * Forgets the registered schemes and destroys the animation free list.
 */
static void* AnimClose(void* instance, RwInt32 offset, RwInt32 size) {
    RtAnimInterpolatorInfoBlockNumEntries = 0;
    if (_rtAnimAnimationFreeList != NULL) {
        RwFreeListDestroy(_rtAnimAnimationFreeList);
        _rtAnimAnimationFreeList = NULL;
    }
    return instance;
} // 0x802F5F70

/*
 * ==================== RtAnimInitialize ====================
 * Registers the toolkit with the engine.
 */
RwBool RtAnimInitialize(void) {
    return RwEngineRegisterPlugin(0, rwID_ANIMTOOLKIT, AnimOpen, AnimClose) > 0;
} // 0x802F5FC4

/*
 * ==================== RtAnimRegisterInterpolationScheme ====================
 * Adds a copy of an interpolation scheme to the table, unless the table is full or a scheme
 * with its typeID is already there.
 */
RwBool RtAnimRegisterInterpolationScheme(RtAnimInterpolatorInfo* interpolatorInfo) {
    RwInt32 i;

    if (RtAnimInterpolatorInfoBlockNumEntries < rtANIMMAXINTERPOLATIONSCHEMES) {
        for (i = 0; i < RtAnimInterpolatorInfoBlockNumEntries; i++) {
            if (interpolatorInfo->typeID == RtAnimInterpolatorInfoBlock[i].typeID) {
                RWERROR((E_RT_ANIM_INTERPIDINUSE));
                return FALSE;
            }
        }

        RtAnimInterpolatorInfoBlock[RtAnimInterpolatorInfoBlockNumEntries] = *interpolatorInfo;
        RtAnimInterpolatorInfoBlockNumEntries++;
        return TRUE;
    }

    RWERROR((E_RT_ANIM_INTERPBLOCKFULL));
    return FALSE;
} // 0x802F6008

/*
 * ==================== RtAnimGetInterpolatorInfo ====================
 * The registered scheme with a typeID, or NULL.
 */
RtAnimInterpolatorInfo* RtAnimGetInterpolatorInfo(RwInt32 typeID) {
    RwInt32 i;

    for (i = 0; i < RtAnimInterpolatorInfoBlockNumEntries; i++) {
        if (typeID == RtAnimInterpolatorInfoBlock[i].typeID) {
            return &RtAnimInterpolatorInfoBlock[i];
        }
    }

    RWERROR((E_RT_ANIM_INTERPIDUNKNOWN));
    return NULL;
} // 0x802F6134

/*
 * ==================== RtAnimAnimationCreate ====================
 * Allocates an animation of numFrames keyframes of a scheme, with its custom data.
 */
RtAnimAnimation* RtAnimAnimationCreate(RwInt32 typeID, RwInt32 numFrames, RwInt32 flags, RwReal duration) {
    RtAnimInterpolatorInfo* interpInfo;
    RtAnimAnimation* animation;

    interpInfo = RtAnimGetInterpolatorInfo(typeID);
    if (interpInfo == NULL) {
        return NULL;
    }

    animation = (RtAnimAnimation*)RwMalloc(sizeof(RtAnimAnimation) + numFrames * interpInfo->animKeyFrameSize +
                                               interpInfo->customDataSize,
                                           rwMEMHINTDUR_EVENT | rwID_HANIMANIMATION);
    animation->numFrames = numFrames;
    animation->duration = duration;
    animation->flags = flags;
    animation->interpInfo = interpInfo;
    animation->pFrames = animation + 1;
    if (interpInfo->customDataSize > 0) {
        animation->customData = (RwUInt8*)animation->pFrames + numFrames * interpInfo->animKeyFrameSize;
    } else {
        animation->customData = NULL;
    }
    return animation;
} // 0x802F61BC

/*
 * ==================== RtAnimAnimationDestroy ====================
 */
RwBool RtAnimAnimationDestroy(RtAnimAnimation* animation) {
    RwFree(animation);
    return TRUE;
} // 0x802F62F0

/*
 * ==================== RtAnimAnimationStreamRead ====================
 * Reads an animation's stream data (after its chunk header) and its keyframes.
 */
RtAnimAnimation* RtAnimAnimationStreamRead(RwStream* stream) {
    RwInt32 version;
    RwInt32 typeID;
    RwInt32 numFrames;
    RwInt32 flags;
    RwReal duration;
    RtAnimInterpolatorInfo* interpInfo;
    RtAnimAnimation* animation;

    if (!RwStreamReadInt32(stream, &version, sizeof(RwInt32))) {
        return NULL;
    }
    if (version != rtANIMSTREAMCURRENTVERSION) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &typeID, sizeof(RwInt32))) {
        return NULL;
    }
    interpInfo = RtAnimGetInterpolatorInfo(typeID);
    if (interpInfo == NULL) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &numFrames, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &flags, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamReadReal(stream, &duration, sizeof(RwReal))) {
        return NULL;
    }

    animation = RtAnimAnimationCreate(typeID, numFrames, flags, duration);
    if (animation == NULL) {
        return NULL;
    }
    interpInfo->keyFrameStreamReadCB(stream, animation);
    return animation;
} // 0x802F6320

/*
 * ==================== RtAnimAnimationStreamWrite ====================
 * Writes an animation as a chunk: its header data, then its keyframes.
 */
RwBool RtAnimAnimationStreamWrite(const RtAnimAnimation* animation, RwStream* stream) {
    RwInt32 version = rtANIMSTREAMCURRENTVERSION;

    RwStreamWriteChunkHeader(stream, rwID_HANIMANIMATION, RtAnimAnimationStreamGetSize(animation));
    if (!RwStreamWriteInt32(stream, &version, sizeof(RwInt32))) {
        return FALSE;
    }
    if (!RwStreamWriteInt32(stream, &animation->interpInfo->typeID, sizeof(RwInt32))) {
        return FALSE;
    }
    if (!RwStreamWriteInt32(stream, &animation->numFrames, sizeof(RwInt32))) {
        return FALSE;
    }
    if (!RwStreamWriteInt32(stream, &animation->flags, sizeof(RwInt32))) {
        return FALSE;
    }
    if (!RwStreamWriteReal(stream, &animation->duration, sizeof(RwReal))) {
        return FALSE;
    }
    animation->interpInfo->keyFrameStreamWriteCB(animation, stream);
    return TRUE;
} // 0x802F65B8

/*
 * ==================== RtAnimAnimationStreamGetSize ====================
 * The size of an animation's chunk data: five header words and the keyframes.
 */
RwInt32 RtAnimAnimationStreamGetSize(const RtAnimAnimation* animation) {
    return animation->interpInfo->keyFrameStreamGetSizeCB(animation) + sizeof(RwInt32) * 4 + sizeof(RwReal);
} // 0x802F66DC

/*
 * ==================== RtAnimAnimationGetNumNodes ====================
 * The number of nodes an animation drives: the number of keyframes before the first one that
 * replaces the first keyframe.
 */
RwUInt32 RtAnimAnimationGetNumNodes(const RtAnimAnimation* animation) {
    RwUInt32 numNodes = 0;
    RwInt32 keyFrameSize = animation->interpInfo->animKeyFrameSize;
    RwUInt8* keyFrame = (RwUInt8*)animation->pFrames;

    while (((RtAnimKeyFrameHeader*)keyFrame)->prevFrame != animation->pFrames) {
        keyFrame += keyFrameSize;
        numNodes++;
    }
    return numNodes;
} // 0x802F670C

/*
 * ==================== RtAnimInterpolatorCreate ====================
 * Allocates an interpolator for numNodes nodes, with room for interpolated keyframes of up to
 * maxInterpKeyFrameSize bytes.
 */
RtAnimInterpolator* RtAnimInterpolatorCreate(RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    RtAnimInterpolator* anim;

    anim = (RtAnimInterpolator*)RwMalloc(sizeof(RtAnimInterpolator) + numNodes * maxInterpKeyFrameSize,
                                         rwMEMHINTDUR_EVENT | rwID_HANIMANIMATION);
    anim->numNodes = numNodes;
    anim->pCurrentAnim = NULL;
    anim->pNextFrame = NULL;
    anim->currentTime = 0.0f;
    anim->pAnimCallBack = NULL;
    anim->animCallBackTime = -1.0f;
    anim->pAnimCallBackData = NULL;
    anim->pAnimLoopCallBack = NULL;
    anim->pAnimLoopCallBackData = NULL;
    anim->currentInterpKeyFrameSize = maxInterpKeyFrameSize;
    anim->currentAnimKeyFrameSize = -1;
    anim->maxInterpKeyFrameSize = maxInterpKeyFrameSize;
    anim->isSubInterpolator = FALSE;
    anim->offsetInParent = 0;
    anim->parentAnimation = anim;
    anim->keyFrameApplyCB = NULL;
    anim->keyFrameInterpolateCB = NULL;
    anim->keyFrameBlendCB = NULL;
    anim->keyFrameAddCB = NULL;
    return anim;
} // 0x802F6740

/*
 * ==================== RtAnimInterpolatorDestroy ====================
 */
void RtAnimInterpolatorDestroy(RtAnimInterpolator* anim) {
    RwFree(anim);
} // 0x802F67F0

/*
 * ==================== RtAnimInterpolatorSetCurrentAnim ====================
 * Starts an animation at time zero: each node lies between its first two keyframes.
 */
RwBool RtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator* animI, RtAnimAnimation* anim) {
    RwInt32 i;
    RwUInt8* keyFrame1;
    RwUInt8* keyFrame2;
    RwUInt8* interpFrame;
    RtAnimInterpolatorInfo* interpInfo;

    animI->pCurrentAnim = anim;
    animI->currentTime = 0.0f;
    interpInfo = anim->interpInfo;
    animI->currentInterpKeyFrameSize = interpInfo->interpKeyFrameSize;
    animI->currentAnimKeyFrameSize = interpInfo->animKeyFrameSize;
    animI->keyFrameApplyCB = interpInfo->keyFrameApplyCB;
    animI->keyFrameBlendCB = interpInfo->keyFrameBlendCB;
    animI->keyFrameInterpolateCB = interpInfo->keyFrameInterpolateCB;
    animI->keyFrameAddCB = interpInfo->keyFrameAddCB;

    for (i = 0; i < animI->numNodes; i++) {
        animI->keyFrameInterpolateCB(rtANIMGETINTERPFRAME(animI, i),
                                     (RwUInt8*)anim->pFrames + i * animI->currentAnimKeyFrameSize,
                                     (RwUInt8*)anim->pFrames + (i + animI->numNodes) * animI->currentAnimKeyFrameSize,
                                     0.0f, anim->customData);
    }

    keyFrame1 = (RwUInt8*)anim->pFrames;
    keyFrame2 = keyFrame1 + animI->numNodes * animI->currentAnimKeyFrameSize;
    interpFrame = (RwUInt8*)rtANIMGETINTERPFRAME(animI, 0);
    for (i = 0; i < animI->numNodes; i++) {
        ((RtAnimInterpFrameHeader*)interpFrame)->keyFrame1 = (RtAnimKeyFrameHeader*)keyFrame1;
        ((RtAnimInterpFrameHeader*)interpFrame)->keyFrame2 = (RtAnimKeyFrameHeader*)keyFrame2;
        keyFrame1 += animI->currentAnimKeyFrameSize;
        keyFrame2 += animI->currentAnimKeyFrameSize;
        interpFrame += animI->currentInterpKeyFrameSize;
    }

    animI->pNextFrame = (RwUInt8*)anim->pFrames + animI->numNodes * animI->currentAnimKeyFrameSize * 2;
    return TRUE;
} // 0x802F6800

/*
 * ==================== RtAnimInterpolatorSetKeyFrameCallBacks ====================
 * Takes the keyframe callbacks of a scheme.
 */
RwBool RtAnimInterpolatorSetKeyFrameCallBacks(RtAnimInterpolator* anim, RwInt32 keyFrameTypeID) {
    RtAnimInterpolatorInfo* interpInfo;

    interpInfo = RtAnimGetInterpolatorInfo(keyFrameTypeID);
    if (interpInfo == NULL) {
        return FALSE;
    }
    anim->keyFrameApplyCB = interpInfo->keyFrameApplyCB;
    anim->keyFrameBlendCB = interpInfo->keyFrameBlendCB;
    anim->keyFrameInterpolateCB = interpInfo->keyFrameInterpolateCB;
    anim->keyFrameAddCB = interpInfo->keyFrameAddCB;
    return TRUE;
} // 0x802F6934

/*
 * ==================== RtAnimInterpolatorSetAnimLoopCallBack ====================
 * Sets the callback called each time the animation loops.
 */
void RtAnimInterpolatorSetAnimLoopCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, void* data) {
    anim->pAnimLoopCallBack = callBack;
    anim->pAnimLoopCallBackData = data;
} // 0x802F69FC

/*
 * ==================== RtAnimInterpolatorCopy ====================
 * Copies the interpolated keyframes of one interpolator to another.
 */
RwBool RtAnimInterpolatorCopy(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim) {
    if (outAnim->currentInterpKeyFrameSize != inAnim->currentInterpKeyFrameSize) {
        outAnim->currentInterpKeyFrameSize = inAnim->currentInterpKeyFrameSize;
    }
    memcpy(rtANIMGETINTERPFRAME(outAnim, 0), rtANIMGETINTERPFRAME(inAnim, 0),
           outAnim->currentInterpKeyFrameSize * outAnim->numNodes);
    return TRUE;
} // 0x802F6A08

/*
 * ==================== RtAnimInterpolatorSubAnimTime ====================
 * Steps the animation back by time. Stepping back past the start wraps to the end and replays
 * from the start; otherwise each node steps back through its keyframes.
 */
RwBool RtAnimInterpolatorSubAnimTime(RtAnimInterpolator* anim, RwReal time) {
    RwBool callBack;
    RwBool looped;
    RtAnimAnimation* currentAnim;
    RwReal duration;
    RwInt32 numNodes;
    RtAnimInterpFrameHeader* interpFrame;

    looped = FALSE;
    callBack = FALSE;
    if (anim->currentTime > anim->animCallBackTime && anim->currentTime - time <= anim->animCallBackTime) {
        callBack = TRUE;
    }

    anim->currentTime -= time;
    currentAnim = anim->pCurrentAnim;
    if (anim->currentTime < 0.0f) {
        duration = currentAnim->duration;
        while (anim->currentTime < 0.0f) {
            anim->currentTime += duration;
        }
        time = anim->currentTime;
        RtAnimInterpolatorSetCurrentAnim(anim, currentAnim);
        RtAnimInterpolatorAddAnimTime(anim, time);
        callBack |= anim->currentTime < anim->animCallBackTime;
        looped = TRUE;
    } else {
        numNodes = anim->numNodes;
        interpFrame = (RtAnimInterpFrameHeader*)rtANIMGETINTERPFRAME(anim, 0);
        while (numNodes--) {
            while (anim->currentTime < interpFrame->keyFrame1->time) {
                interpFrame->keyFrame2 = interpFrame->keyFrame1;
                interpFrame->keyFrame1 = (RtAnimKeyFrameHeader*)interpFrame->keyFrame1->prevFrame;
            }
            RtAnimKeyFrameInterpolate(anim, interpFrame, interpFrame->keyFrame1, interpFrame->keyFrame2,
                                      anim->currentTime);
            interpFrame = (RtAnimInterpFrameHeader*)((RwUInt8*)interpFrame + anim->currentInterpKeyFrameSize);
        }
        anim->pNextFrame = NULL;
    }

    if (callBack && anim->pAnimCallBack != NULL) {
        if (anim->pAnimCallBack(anim, anim->pAnimCallBackData) == NULL) {
            anim->pAnimCallBack = NULL;
        }
    }
    if (looped && anim->pAnimLoopCallBack != NULL) {
        if (anim->pAnimLoopCallBack(anim, anim->pAnimLoopCallBackData) == NULL) {
            anim->pAnimLoopCallBack = NULL;
        }
    }
    return TRUE;
} // 0x802F6A54

/*
 * ==================== RtAnimInterpolatorAddAnimTime ====================
 * Steps the animation on by time, looping past its end, and interpolates every node.
 */
RwBool RtAnimInterpolatorAddAnimTime(RtAnimInterpolator* anim, RwReal time) {
    RwInt32 numNodes;
    RwBool callBack;
    RwBool looped;
    RtAnimAnimation* currentAnim;
    RwReal duration;
    RwInt32 i;
    void* lastFrame;
    RtAnimKeyFrameHeader* nextFrame;
    RtAnimKeyFrameHeader* prevFrame;
    RtAnimInterpFrameHeader* interpFrame;

    if (time > 0.0f) {
        looped = FALSE;
        callBack = FALSE;
        currentAnim = anim->pCurrentAnim;
        numNodes = anim->numNodes;
        duration = currentAnim->duration;

        if (anim->currentTime < anim->animCallBackTime && anim->currentTime + time >= anim->animCallBackTime) {
            callBack = TRUE;
        }

        anim->currentTime += time;
        if (anim->currentTime > duration) {
            while (anim->currentTime > duration) {
                looped = TRUE;
                anim->currentTime -= duration;
                callBack |= anim->currentTime >= anim->animCallBackTime;
            }
            time = anim->currentTime;
            RtAnimInterpolatorSetCurrentAnim(anim, currentAnim);
            anim->currentTime = time;
        }

        if (anim->pNextFrame == NULL) {
            time = anim->currentTime;
            RtAnimInterpolatorSetCurrentAnim(anim, currentAnim);
            anim->currentTime = time;
        }

        /* Bring in each keyframe whose predecessor has ended, at the node that holds the predecessor. */
        i = 0;
        lastFrame = (RwUInt8*)currentAnim->pFrames + currentAnim->numFrames * anim->currentAnimKeyFrameSize;
        while (anim->pNextFrame < lastFrame) {
            nextFrame = (RtAnimKeyFrameHeader*)anim->pNextFrame;
            prevFrame = (RtAnimKeyFrameHeader*)nextFrame->prevFrame;
            if (prevFrame->time > anim->currentTime) {
                break;
            }
            do {
                interpFrame = (RtAnimInterpFrameHeader*)rtANIMGETINTERPFRAME(anim, i);
                i++;
                if (i >= numNodes) {
                    i = 0;
                }
            } while (interpFrame->keyFrame2 != prevFrame);
            interpFrame->keyFrame1 = interpFrame->keyFrame2;
            interpFrame->keyFrame2 = (RtAnimKeyFrameHeader*)anim->pNextFrame;
            anim->pNextFrame = (RwUInt8*)anim->pNextFrame + anim->currentAnimKeyFrameSize;
        }

        interpFrame = (RtAnimInterpFrameHeader*)rtANIMGETINTERPFRAME(anim, 0);
        for (i = 0; i < numNodes; i++) {
            RtAnimKeyFrameInterpolate(anim, interpFrame, interpFrame->keyFrame1, interpFrame->keyFrame2,
                                      anim->currentTime);
            interpFrame = (RtAnimInterpFrameHeader*)((RwUInt8*)interpFrame + anim->currentInterpKeyFrameSize);
        }

        if (callBack && anim->pAnimCallBack != NULL) {
            if (anim->pAnimCallBack(anim, anim->pAnimCallBackData) == NULL) {
                anim->pAnimCallBack = NULL;
            }
        }
        if (looped && anim->pAnimLoopCallBack != NULL) {
            if (anim->pAnimLoopCallBack(anim, anim->pAnimLoopCallBackData) == NULL) {
                anim->pAnimLoopCallBack = NULL;
            }
        }
    }
    return TRUE;
} // 0x802F6C08

/*
 * ==================== RtAnimInterpolatorSetCurrentTime ====================
 * Steps the animation to a time, back or on.
 */
RwBool RtAnimInterpolatorSetCurrentTime(RtAnimInterpolator* anim, RwReal time) {
    time -= anim->currentTime;
    if (time < 0.0f) {
        RtAnimInterpolatorSubAnimTime(anim, -time);
    } else {
        RtAnimInterpolatorAddAnimTime(anim, time);
    }
    return TRUE;
} // 0x802F6E40

/*
 * ==================== RtAnimInterpolatorBlend ====================
 * Blends the interpolated keyframes of two interpolators by alpha.
 */
RwBool RtAnimInterpolatorBlend(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1,
                               RtAnimInterpolator* inAnim2, RwReal alpha) {
    RwInt32 i;

    if (outAnim->currentInterpKeyFrameSize != inAnim1->currentInterpKeyFrameSize) {
        outAnim->currentInterpKeyFrameSize = inAnim1->currentInterpKeyFrameSize;
    }
    for (i = 0; i < outAnim->numNodes; i++) {
        RtAnimKeyFrameBlend(outAnim, rtANIMGETINTERPFRAME(outAnim, i), rtANIMGETINTERPFRAME(inAnim1, i),
                            rtANIMGETINTERPFRAME(inAnim2, i), alpha);
    }
    return TRUE;
} // 0x802F6E84

/*
 * ==================== RtAnimInterpolatorAddTogether ====================
 * Adds the interpolated keyframes of two interpolators.
 */
RwBool RtAnimInterpolatorAddTogether(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1,
                                     RtAnimInterpolator* inAnim2) {
    RwInt32 i;

    for (i = 0; i < outAnim->numNodes; i++) {
        RtAnimKeyFrameAddTogether(outAnim, rtANIMGETINTERPFRAME(outAnim, i), rtANIMGETINTERPFRAME(inAnim1, i),
                                  rtANIMGETINTERPFRAME(inAnim2, i));
    }
    return TRUE;
} // 0x802F6F48

/*
 * ==================== RtAnimInterpolatorCreateSubInterpolator ====================
 * Creates an interpolator for numNodes of a parent's nodes, from startNode (-1 as the keyframe
 * size takes the parent's).
 */
RtAnimInterpolator* RtAnimInterpolatorCreateSubInterpolator(RtAnimInterpolator* parentAnim, RwInt32 startNode,
                                                            RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize) {
    RtAnimInterpolator* subAnim;

    if (maxInterpKeyFrameSize == -1) {
        maxInterpKeyFrameSize = parentAnim->maxInterpKeyFrameSize;
    }
    subAnim = RtAnimInterpolatorCreate(numNodes, maxInterpKeyFrameSize);
    subAnim->parentAnimation = parentAnim;
    subAnim->offsetInParent = startNode;
    subAnim->isSubInterpolator = TRUE;
    return subAnim;
} // 0x802F6FE8

/*
 * ==================== RtAnimInterpolatorBlendSubInterpolator ====================
 * Blends two interpolators of which one or more drives only part of the nodes: the nodes only
 * the full interpolator drives are copied from it.
 */
RwBool RtAnimInterpolatorBlendSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1,
                                              RtAnimInterpolator* inAnim2, RwReal alpha) {
    RwInt32 outOffset;
    RwInt32 i;
    RwInt32 in1Offset;
    RwInt32 in2Offset;

    if (outAnim->currentInterpKeyFrameSize != inAnim1->currentInterpKeyFrameSize) {
        outAnim->currentInterpKeyFrameSize = inAnim1->currentInterpKeyFrameSize;
    }

    if (outAnim->isSubInterpolator == TRUE) {
        outOffset = outAnim->offsetInParent;
        in1Offset = inAnim1->offsetInParent;
        in2Offset = inAnim2->offsetInParent;

        for (i = outOffset; i < outAnim->numNodes + outAnim->offsetInParent; i++) {
            RtAnimKeyFrameBlend(outAnim, rtANIMGETINTERPFRAME(outAnim, i - outOffset),
                                rtANIMGETINTERPFRAME(inAnim1, i - in1Offset),
                                rtANIMGETINTERPFRAME(inAnim2, i - in2Offset), alpha);
        }
    } else if (inAnim1->isSubInterpolator == TRUE) {
        memcpy(rtANIMGETINTERPFRAME(outAnim, 0), rtANIMGETINTERPFRAME(inAnim2, 0),
               outAnim->currentInterpKeyFrameSize * inAnim1->offsetInParent);
        for (i = inAnim1->offsetInParent; i < inAnim1->offsetInParent + inAnim1->numNodes; i++) {
            RtAnimKeyFrameBlend(outAnim, rtANIMGETINTERPFRAME(outAnim, i),
                                rtANIMGETINTERPFRAME(inAnim1, i - inAnim1->offsetInParent),
                                rtANIMGETINTERPFRAME(inAnim2, i), alpha);
        }
        memcpy(rtANIMGETINTERPFRAME(outAnim, inAnim1->offsetInParent + inAnim1->numNodes),
               rtANIMGETINTERPFRAME(inAnim2, inAnim1->offsetInParent + inAnim1->numNodes),
               outAnim->currentInterpKeyFrameSize *
                   (outAnim->numNodes - (inAnim1->offsetInParent + inAnim1->numNodes)));
    } else {
        memcpy(rtANIMGETINTERPFRAME(outAnim, 0), rtANIMGETINTERPFRAME(inAnim1, 0),
               outAnim->currentInterpKeyFrameSize * inAnim2->offsetInParent);
        for (i = inAnim2->offsetInParent; i < inAnim2->offsetInParent + inAnim2->numNodes; i++) {
            RtAnimKeyFrameBlend(outAnim, rtANIMGETINTERPFRAME(outAnim, i), rtANIMGETINTERPFRAME(inAnim1, i),
                                rtANIMGETINTERPFRAME(inAnim2, i - inAnim2->offsetInParent), alpha);
        }
        memcpy(rtANIMGETINTERPFRAME(outAnim, inAnim2->offsetInParent + inAnim2->numNodes),
               rtANIMGETINTERPFRAME(inAnim1, inAnim2->offsetInParent + inAnim2->numNodes),
               outAnim->currentInterpKeyFrameSize *
                   (outAnim->numNodes - (inAnim2->offsetInParent + inAnim2->numNodes)));
    }
    return TRUE;
} // 0x802F7044
