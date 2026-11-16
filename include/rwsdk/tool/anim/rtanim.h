// Reference: tgsm/pbg config/GPLE9G/symbols.txt (function names)
// Reference: bfbbdecomp/bfbb symbols.txt (RtAnimInterpolatorInfoBlock, RtAnimInterpolatorInfoBlockNumEntries)
#ifndef RWSDK_TOOL_ANIM_RTANIM_H
#define RWSDK_TOOL_ANIM_RTANIM_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bastream.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RtAnim: the keyframe animation toolkit. An interpolation scheme (RtAnimInterpolatorInfo,
 * registered by a plugin such as HAnim or UV animation) describes one keyframe type; an
 * RtAnimAnimation holds the keyframes of one animation in that type, and an RtAnimInterpolator
 * plays an animation over a number of nodes, keeping one interpolated keyframe per node.
 */

/* The version written in the stream data of an animation. */
#define rtANIMSTREAMCURRENTVERSION 0x100

/* The number of interpolation schemes the toolkit can hold. Name invented. */
#define rtANIMMAXINTERPOLATIONSCHEMES 16

/* RtAnim's error codes (its rtanim.rpe is not public: names invented, values from retail's _rwerror calls). */
enum RtAnimError {
    E_RT_ANIM_INTERPIDINUSE = 0,    /* the scheme's typeID is already registered */
    E_RT_ANIM_INTERPBLOCKFULL = 1,  /* the scheme table is full */
    E_RT_ANIM_INTERPIDUNKNOWN = 2   /* no registered scheme has the typeID */
};

typedef struct RtAnimAnimation RtAnimAnimation;

/* Converts an interpolated keyframe to the result the scheme drives (a matrix, a UV transform...). */
typedef void (*RtAnimKeyFrameApplyCallBack)(void* result, void* voidIFrame);
/* Blends two interpolated keyframes by alpha into voidOut. */
typedef void (*RtAnimKeyFrameBlendCallBack)(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha);
/* Interpolates between two animation keyframes at a time into an interpolated keyframe. */
typedef void (*RtAnimKeyFrameInterpolateCallBack)(void* voidOut, void* voidIn1, void* voidIn2,
                                                  RwReal time, void* customData);
/* Adds two interpolated keyframes into voidOut. */
typedef void (*RtAnimKeyFrameAddCallBack)(void* voidOut, void* voidIn1, void* voidIn2);
/* Multiplies a keyframe by the reciprocal of another (used to make delta animations). */
typedef void (*RtAnimKeyFrameMulRecipCallBack)(void* voidFrame, void* voidStart);
/* Reads, writes and sizes an animation's keyframes in a stream. */
typedef RtAnimAnimation* (*RtAnimKeyFrameStreamReadCallBack)(RwStream* stream, RtAnimAnimation* animation);
typedef RwBool (*RtAnimKeyFrameStreamWriteCallBack)(const RtAnimAnimation* animation, RwStream* stream);
typedef RwInt32 (*RtAnimKeyFrameStreamGetSizeCallBack)(const RtAnimAnimation* animation);

/* RtAnimInterpolatorInfo: an interpolation scheme, the keyframe type of a plugin. */
typedef struct RtAnimInterpolatorInfo {
    RwInt32 typeID;                                              /* 0x0: the scheme's ID */
    RwInt32 interpKeyFrameSize;                                  /* 0x4: bytes of an interpolated keyframe */
    RwInt32 animKeyFrameSize;                                    /* 0x8: bytes of an animation keyframe */
    RtAnimKeyFrameApplyCallBack keyFrameApplyCB;                 /* 0xC */
    RtAnimKeyFrameBlendCallBack keyFrameBlendCB;                 /* 0x10 */
    RtAnimKeyFrameInterpolateCallBack keyFrameInterpolateCB;     /* 0x14 */
    RtAnimKeyFrameAddCallBack keyFrameAddCB;                     /* 0x18 */
    RtAnimKeyFrameMulRecipCallBack keyFrameMulRecipCB;           /* 0x1C */
    RtAnimKeyFrameStreamReadCallBack keyFrameStreamReadCB;       /* 0x20 */
    RtAnimKeyFrameStreamWriteCallBack keyFrameStreamWriteCB;     /* 0x24 */
    RtAnimKeyFrameStreamGetSizeCallBack keyFrameStreamGetSizeCB; /* 0x28 */
    RwInt32 customDataSize;                                      /* 0x2C: bytes of custom data per animation */
    RwInt32 field_0x30;                                          /* 0x30: not in the 3.7.0.2 header; purpose unknown (only copied here) */
} RtAnimInterpolatorInfo; /* size 0x34 */

/*
 * RtAnimAnimation: the keyframes of one animation. The keyframes start with one per node at time
 * zero; each later keyframe follows the one it replaces (its prevFrame), in order of the time
 * that keyframe is needed. The keyframes and the custom data follow the structure in one block.
 */
struct RtAnimAnimation {
    RtAnimInterpolatorInfo* interpInfo; /* 0x0 */
    RwInt32 numFrames;                  /* 0x4 */
    RwInt32 flags;                      /* 0x8 */
    RwReal duration;                    /* 0xC: seconds */
    void* pFrames;                      /* 0x10: the keyframes */
    void* customData;                   /* 0x14: NULL when the scheme has none */
}; /* size 0x18 */

/* RtAnimKeyFrameHeader: the start of every animation keyframe. */
typedef struct RtAnimKeyFrameHeader {
    void* prevFrame; /* 0x0: the node's previous keyframe */
    RwReal time;     /* 0x4 */
} RtAnimKeyFrameHeader;

/* RtAnimInterpFrameHeader: the start of every interpolated keyframe: the pair it lies between. */
typedef struct RtAnimInterpFrameHeader {
    RtAnimKeyFrameHeader* keyFrame1; /* 0x0 */
    RtAnimKeyFrameHeader* keyFrame2; /* 0x4 */
} RtAnimInterpFrameHeader;

typedef struct RtAnimInterpolator RtAnimInterpolator;

/* An animation callback; returning NULL removes it. */
typedef RtAnimInterpolator* (*RtAnimCallBack)(RtAnimInterpolator* animInstance, void* data);

/*
 * RtAnimInterpolator: plays an animation over numNodes nodes. The interpolated keyframes, one per
 * node, follow the structure (rtANIMGETINTERPFRAME). A sub-interpolator drives the nodes
 * offsetInParent.. of its parent's.
 */
struct RtAnimInterpolator {
    RtAnimAnimation* pCurrentAnim;                     /* 0x0 */
    RwReal currentTime;                                /* 0x4 */
    void* pNextFrame;                                  /* 0x8: the next keyframe needed; NULL to restart */
    RtAnimCallBack pAnimCallBack;                      /* 0xC */
    void* pAnimCallBackData;                           /* 0x10 */
    RwReal animCallBackTime;                           /* 0x14 */
    RtAnimCallBack pAnimLoopCallBack;                  /* 0x18 */
    void* pAnimLoopCallBackData;                       /* 0x1C */
    RwInt32 maxInterpKeyFrameSize;                     /* 0x20 */
    RwInt32 currentInterpKeyFrameSize;                 /* 0x24 */
    RwInt32 currentAnimKeyFrameSize;                   /* 0x28 */
    RwInt32 numNodes;                                  /* 0x2C */
    RwBool isSubInterpolator;                          /* 0x30 */
    RwInt32 offsetInParent;                            /* 0x34 */
    RtAnimInterpolator* parentAnimation;               /* 0x38 */
    RtAnimKeyFrameApplyCallBack keyFrameApplyCB;       /* 0x3C */
    RtAnimKeyFrameBlendCallBack keyFrameBlendCB;       /* 0x40 */
    RtAnimKeyFrameInterpolateCallBack keyFrameInterpolateCB; /* 0x44 */
    RtAnimKeyFrameAddCallBack keyFrameAddCB;           /* 0x48 */
}; /* size 0x4C */

/* The interpolated keyframe of a node. */
#define rtANIMGETINTERPFRAME(anim, nodeIndex) \
    ((void*)(((RwUInt8*)&((anim)[1]) + ((nodeIndex) * (anim)->currentInterpKeyFrameSize))))

/* The registered interpolation schemes. */
extern RtAnimInterpolatorInfo RtAnimInterpolatorInfoBlock[rtANIMMAXINTERPOLATIONSCHEMES];
extern RwInt32 RtAnimInterpolatorInfoBlockNumEntries;

void RtAnimAnimationFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc);
RwBool RtAnimInitialize(void);
RwBool RtAnimRegisterInterpolationScheme(RtAnimInterpolatorInfo* interpolatorInfo);
RtAnimInterpolatorInfo* RtAnimGetInterpolatorInfo(RwInt32 typeID);

RtAnimAnimation* RtAnimAnimationCreate(RwInt32 typeID, RwInt32 numFrames, RwInt32 flags, RwReal duration);
RwBool RtAnimAnimationDestroy(RtAnimAnimation* animation);
RtAnimAnimation* RtAnimAnimationRead(const RwChar* filename);
RwBool RtAnimAnimationWrite(const RtAnimAnimation* animation, const RwChar* filename);
RtAnimAnimation* RtAnimAnimationStreamRead(RwStream* stream);
RwBool RtAnimAnimationStreamWrite(const RtAnimAnimation* animation, RwStream* stream);
RwInt32 RtAnimAnimationStreamGetSize(const RtAnimAnimation* animation);
RwUInt32 RtAnimAnimationGetNumNodes(const RtAnimAnimation* animation);
#define RtAnimAnimationGetTypeID(animation) ((animation)->interpInfo->typeID)

RtAnimInterpolator* RtAnimInterpolatorCreate(RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize);
void RtAnimInterpolatorDestroy(RtAnimInterpolator* anim);
RwBool RtAnimInterpolatorSetCurrentAnim(RtAnimInterpolator* animI, RtAnimAnimation* anim);
#define RtAnimInterpolatorGetCurrentAnim(animI) ((animI)->pCurrentAnim)
RwBool RtAnimInterpolatorSetKeyFrameCallBacks(RtAnimInterpolator* anim, RwInt32 keyFrameTypeID);
void RtAnimInterpolatorSetAnimLoopCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, void* data);
void RtAnimInterpolatorSetAnimCallBack(RtAnimInterpolator* anim, RtAnimCallBack callBack, RwReal time,
                                       void* data);
RwBool RtAnimInterpolatorCopy(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim);
RwBool RtAnimInterpolatorSubAnimTime(RtAnimInterpolator* anim, RwReal time);
RwBool RtAnimInterpolatorAddAnimTime(RtAnimInterpolator* anim, RwReal time);
RwBool RtAnimInterpolatorSetCurrentTime(RtAnimInterpolator* anim, RwReal time);
RwBool RtAnimAnimationMakeDelta(RtAnimAnimation* animation, RwInt32 numNodes, RwReal time);
RwBool RtAnimInterpolatorBlend(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1,
                               RtAnimInterpolator* inAnim2, RwReal alpha);
RwBool RtAnimInterpolatorAddTogether(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1,
                                     RtAnimInterpolator* inAnim2);

/* Calls of an interpolator's keyframe callbacks. */
#define RtAnimKeyFrameApply(anim, out, in) \
    do { (anim)->keyFrameApplyCB((out), (in)); } while (0)
#define RtAnimKeyFrameInterpolate(anim, out, in1, in2, time) \
    do { (anim)->keyFrameInterpolateCB((out), (in1), (in2), (time), (anim)->pCurrentAnim->customData); } while (0)
#define RtAnimKeyFrameBlend(anim, out, in1, in2, fAlpha) \
    do { (anim)->keyFrameBlendCB((out), (in1), (in2), (fAlpha)); } while (0)
#define RtAnimKeyFrameAddTogether(anim, out, in1, in2) \
    do { (anim)->keyFrameAddCB((out), (in1), (in2)); } while (0)

RtAnimInterpolator* RtAnimInterpolatorCreateSubInterpolator(RtAnimInterpolator* parentAnim, RwInt32 startNode,
                                                            RwInt32 numNodes, RwInt32 maxInterpKeyFrameSize);
RwBool RtAnimInterpolatorBlendSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* inAnim1,
                                              RtAnimInterpolator* inAnim2, RwReal alpha);
RwBool RtAnimInterpolatorAddSubInterpolator(RtAnimInterpolator* outAnim, RtAnimInterpolator* mainAnim,
                                            RtAnimInterpolator* subAnim);

#ifdef __cplusplus
}
#endif

#endif
