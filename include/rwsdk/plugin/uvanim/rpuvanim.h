// Layout and names: Gormiti DWARF and link map (RpUVAnim.a linear, param, rpuvanim and stdkey objects)
#ifndef RWSDK_PLUGIN_UVANIM_RPUVANIM_H
#define RWSDK_PLUGIN_UVANIM_RPUVANIM_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/tool/anim/rtanim.h>
#include <rwsdk/tool/dict/rtdict.h>
#include <rwsdk/world/bamateri.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RpUVAnim: UV animation. A material animates up to two UV matrices (one per texture channel).
 * Each channel's matrix is driven by an interpolator over the node the animation's custom data
 * maps to that channel; the keyframes of the two schemes below turn into the matrix.
 */

/* The plugin's ID, in the memory hints of its allocations and in its stream chunks. */
#ifndef rwID_UVANIMPLUGIN
#define rwID_UVANIMPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x35)
#endif

/* The chunk ID of a UV animation dictionary. */
#ifndef rwID_UVANIMDICT
#define rwID_UVANIMDICT MAKECHUNKID(rwVENDORID_CORE, 0x2B)
#endif

/* A UV animation is an RtAnim animation whose custom data is an _rpUVAnimCustomData. */
typedef RtAnimAnimation RpUVAnim;

/* The keyframe scheme of a UV animation. */
enum RpUVAnimKeyFrameType {
    rpUVANIMLINEARKEYFRAMES = 0,
    rpUVANIMPARAMKEYFRAMES = 1,
    rpUVANIMFORCEENUMSIZEINT = 0x7FFFFFFF
};
typedef enum RpUVAnimKeyFrameType RpUVAnimKeyFrameType;

/* The keyframe schemes' IDs in rtanim's table: a matrix given as its six free coefficients, and as parameters. */
#define rpUVANIMLINEARKEYFRAMETYPEID 0x1C0
#define rpUVANIMPARAMKEYFRAMETYPEID 0x1C1

/* The size of an animation's name, the channels a material has UV matrices for and the nodes an animation has. */
#define rpUVANIMNAMESIZE 32
#define rpUVANIMMAXCHANNELS 2
#define rpUVANIMMAXNODES 8

/* The linear scheme's keyframe data: the matrix's six free coefficients (right.x, right.y, up.x, up.y, pos.x, pos.y). */
typedef struct RpUVAnimLinearKeyFrameData RpUVAnimLinearKeyFrameData;
struct RpUVAnimLinearKeyFrameData {
    RwReal uv[6]; /* 0x0 */
}; /* size 0x18 */

/* The parametric scheme's keyframe data: rotation, scale, skew and translation. */
typedef struct RpUVAnimParamKeyFrameData RpUVAnimParamKeyFrameData;
struct RpUVAnimParamKeyFrameData {
    RwReal theta; /* 0x0 */
    RwReal s0;    /* 0x4 */
    RwReal s1;    /* 0x8 */
    RwReal skew;  /* 0xC */
    RwReal x;     /* 0x10 */
    RwReal y;     /* 0x14 */
}; /* size 0x18 */

typedef union RpUVAnimKeyFrameData RpUVAnimKeyFrameData;
union RpUVAnimKeyFrameData {
    RpUVAnimLinearKeyFrameData linear; /* 0x0 */
    RpUVAnimParamKeyFrameData param;   /* 0x0 */
}; /* size 0x18 */

/* An animation keyframe of either scheme. */
typedef struct RpUVAnimKeyFrame RpUVAnimKeyFrame;
struct RpUVAnimKeyFrame {
    RpUVAnimKeyFrame* prevFrame;  /* 0x0 */
    RwReal time;                  /* 0x4 */
    RpUVAnimKeyFrameData data;    /* 0x8 */
}; /* size 0x20 */

/* The interpolated keyframe: the pair it lies between and the data between them. */
typedef struct RpUVAnimInterpFrame RpUVAnimInterpFrame;
struct RpUVAnimInterpFrame {
    RpUVAnimKeyFrame* keyFrame1;  /* 0x0 */
    RpUVAnimKeyFrame* keyFrame2;  /* 0x4 */
    RpUVAnimKeyFrameData data;    /* 0x8 */
}; /* size 0x20 */

/* The custom data of a UV animation: its name, which UV channel each node drives and its reference count. */
typedef struct _rpUVAnimCustomData _rpUVAnimCustomData;
struct _rpUVAnimCustomData {
    RwChar name[rpUVANIMNAMESIZE];            /* 0x0 */
    RwUInt32 nodeToUVChannelMap[rpUVANIMMAXNODES]; /* 0x20 */
    RwUInt32 refCount;                        /* 0x40 */
}; /* size 0x44 */

/*
 * The plugin's data in a material: the UV matrix of each of the two channels (made when an
 * animation maps a node to the channel) and the interpolator of each of up to eight animations.
 */
typedef struct RpUVAnimMaterialExtension RpUVAnimMaterialExtension;
struct RpUVAnimMaterialExtension {
    RwMatrix* uv[rpUVANIMMAXCHANNELS];            /* 0x0 */
    RtAnimInterpolator* interp[rpUVANIMMAXNODES]; /* 0x8 */
}; /* size 0x28 */

/* The plugin's globals (rpuvanim.c): its module, the offset of its data in a material, and the free list of custom data. */
extern RwModuleInfo rpUVAnimModule;
extern RwInt32 RpUVAnimMaterialGlobals;
extern RwInt32 _rpUVAnimCustomDataFreeListBlockSize;
extern RwInt32 _rpUVAnimCustomDataFreeListPreallocBlocks;
extern RwFreeList* _rpUVAnimCustomDataFreeList;

/* The plugin's data in a material. Macro name invented. */
#define RPUVANIMMATERIALGETDATA(material) \
    ((RpUVAnimMaterialExtension*)(((RwUInt8*)(material)) + RpUVAnimMaterialGlobals))

/* The dictionary schema of UV animations (rpuvanim.c). */
extern RtDictSchema _rpUVAnimDictSchema;

/* The point the parametric scheme rotates and scales about, and its negative (rpuvanim.c). */
extern const RwV3d rpUVAnimPivot;
extern const RwV3d rpUVAnimInvPivot;

/* The keyframe schemes' registration data (rpuvanim.c). */
extern RtAnimInterpolatorInfo _rpUVAnimLinearInterpolatorInfo;
extern RtAnimInterpolatorInfo _rpUVAnimParamInterpolatorInfo;

/* The linear scheme's callbacks (linear.c). */
void RpUVAnimLinearKeyFrameApply(void* pMatrix, void* pVoidIFrame);
void RpUVAnimLinearKeyFrameInterpolate(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal time, void* customData);
void RpUVAnimLinearKeyFrameBlend(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal alpha);
void RpUVAnimLinearKeyFrameMulRecip(void* pVoidFrame, void* pVoidStart);
void RpUVAnimLinearKeyFrameAdd(void* pVoidOut, void* pVoidIn1, void* pVoidIn2);
RpUVAnimLinearKeyFrameData* RpUVAnimLinearKeyFrameDataInitFromMatrix(RpUVAnimLinearKeyFrameData* data,
                                                                     const RwMatrix* matrix);

/* The parametric scheme's callbacks (param.c). */
void RpUVAnimParamKeyFrameApply(void* pMatrix, void* pVoidIFrame);
void RpUVAnimParamKeyFrameInterpolate(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal time, void* customData);
void RpUVAnimParamKeyFrameBlend(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal alpha);
void RpUVAnimParamKeyFrameMulRecip(void* pVoidFrame, void* pVoidStart);
void RpUVAnimParamKeyFrameAdd(void* pVoidOut, void* pVoidIn1, void* pVoidIn2);
RpUVAnimParamKeyFrameData* RpUVAnimParamKeyFrameDataInitFromMatrix(RpUVAnimParamKeyFrameData* data,
                                                                   const RwMatrix* matrix);

/* The schemes' stream callbacks and keyframe constructor (stdkey.c). */
RpUVAnimKeyFrame* RpUVAnimKeyFrameInit(const RtAnimAnimation* animation, RpUVAnimKeyFrame* keyFrame,
                                       RpUVAnimKeyFrame* prevFrame, RwReal time, RwMatrix* matrix);
RtAnimAnimation* RpUVAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* pAnimation);
RwBool RpUVAnimKeyFrameStreamWrite(const RtAnimAnimation* pAnimation, RwStream* stream);
RwInt32 RpUVAnimKeyFrameStreamGetSize(const RtAnimAnimation* pAnimation);

/* The custom data's stream functions (rpuvanim.c). */
_rpUVAnimCustomData* _rpUVAnimCustomDataStreamRead(RwStream* stream);
const _rpUVAnimCustomData* _rpUVAnimCustomDataStreamWrite(const _rpUVAnimCustomData* customData, RwStream* stream);
RwInt32 _rpUVAnimCustomDataStreamGetSize(const _rpUVAnimCustomData* customData);

/* The plugin's public functions (rpuvanim.c). */
RwBool RpUVAnimPluginAttach(void);
RpUVAnim* RpUVAnimCreate(const RwChar* name, RwUInt32 numNodes, RwUInt32 numFrames, RwReal duration,
                         RwUInt32* nodeIndexToUVChannelMap, RpUVAnimKeyFrameType keyframeType);
RwBool RpUVAnimDestroy(RpUVAnim* anim);
RpUVAnim* RpUVAnimAddRef(RpUVAnim* anim);
RwChar* RpUVAnimGetName(RpUVAnim* anim);
RpMaterial* RpMaterialUVAnimApplyUpdate(RpMaterial* material);
RtAnimInterpolator* RpMaterialUVAnimGetInterpolator(const RpMaterial* material, RwUInt32 uvIndex);
RpMaterial* RpMaterialUVAnimSetCurrentTime(RpMaterial* material, RwReal time);
RpMaterial* RpMaterialUVAnimAddAnimTime(RpMaterial* material, RwReal deltaTime);
RwBool RpMaterialUVAnimExists(const RpMaterial* material);

#ifdef __cplusplus
}
#endif

#endif
