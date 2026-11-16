// Layout and names: Gormiti DWARF (RpHAnim.a rphanim.obj and stdkey.obj)
#ifndef RWSDK_PLUGIN_HANIM_RPHANIM_H
#define RWSDK_PLUGIN_HANIM_RPHANIM_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/baframe.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/tool/anim/rtanim.h>
#include <rwsdk/tool/quat/rtquat.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RpHAnim: hierarchical animation. A hierarchy ties an animation interpolator to a tree of
 * frames: each node of the hierarchy is a frame with an ID, and the interpolator's keyframes
 * (one per node, a rotation and a translation) become the frames' matrices.
 */

/* The plugin's ID, in the memory hints of its allocations and in its stream chunks. */
#ifndef rwID_HANIMPLUGIN
#define rwID_HANIMPLUGIN MAKECHUNKID(rwVENDORID_CRITERIONTK, 0x1E)
#endif

/* The version of the plugin's stream data. */
#define rpHANIMSTREAMCURRENTVERSION 0x100

/* The standard keyframe scheme: its ID in rtanim's table and the size of its keyframes. */
#define rpHANIMSTDKEYFRAMETYPEID 0x1
#define rpHANIMSTDKEYFRAMESIZE sizeof(RpHAnimKeyFrame)

/* A node's flags: how the parent matrix stack moves around it. */
#define rpHANIMPOPPARENTMATRIX 0x01
#define rpHANIMPUSHPARENTMATRIX 0x02

/* The flags of a hierarchy. */
enum RpHAnimHierarchyFlag {
    rpHANIMHIERARCHYSUBHIERARCHY = 0x01,                /* a sub-hierarchy of another hierarchy */
    rpHANIMHIERARCHYNOMATRICES = 0x02,                  /* no matrix array of its own */
    rpHANIMHIERARCHYUPDATEMODELLINGMATRICES = 0x1000,   /* updating writes the frames' modelling matrices */
    rpHANIMHIERARCHYUPDATELTMS = 0x2000,                /* updating writes the frames' LTMs */
    rpHANIMHIERARCHYLOCALSPACEMATRICES = 0x4000,        /* the matrices are in the hierarchy's local space */
    rpHANIMHIERARCHYFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
};
typedef enum RpHAnimHierarchyFlag RpHAnimHierarchyFlag;

/* The standard keyframe: a rotation and a translation at a time. */
typedef struct RpHAnimKeyFrame RpHAnimKeyFrame;
struct RpHAnimKeyFrame {
    RpHAnimKeyFrame* prevFrame; /* 0x0 */
    RwReal time;                /* 0x4 */
    RtQuat q;                   /* 0x8 */
    RwV3d t;                    /* 0x18 */
}; /* size 0x24 */

/* The interpolated standard keyframe: the pair it lies between and the rotation and translation. */
typedef struct RpHAnimInterpFrame {
    RpHAnimKeyFrame* keyFrame1; /* 0x0 */
    RpHAnimKeyFrame* keyFrame2; /* 0x4 */
    RtQuat q;                   /* 0x8 */
    RwV3d t;                    /* 0x18 */
} RpHAnimInterpFrame; /* size 0x24 */

/* One node of a hierarchy: the frame with its ID, its index in the matrix array and its flags. */
typedef struct RpHAnimNodeInfo {
    RwInt32 nodeID;     /* 0x0 */
    RwInt32 nodeIndex;  /* 0x4 */
    RwInt32 flags;      /* 0x8 */
    RwFrame* pFrame;    /* 0xC */
} RpHAnimNodeInfo; /* size 0x10 */

typedef struct RpHAnimHierarchy RpHAnimHierarchy;

/* A hierarchy: its nodes' frames, the matrices animation produced for them and the animation. */
struct RpHAnimHierarchy {
    RwInt32 flags;                      /* 0x0 */
    RwInt32 numNodes;                   /* 0x4 */
    RwMatrix* pMatrixArray;             /* 0x8: numNodes matrices, aligned */
    void* pMatrixArrayUnaligned;        /* 0xC: the allocation pMatrixArray was aligned within */
    RpHAnimNodeInfo* pNodeInfo;         /* 0x10 */
    RwFrame* parentFrame;               /* 0x14 */
    RpHAnimHierarchy* parentHierarchy;  /* 0x18 */
    RwInt32 rootParentOffset;           /* 0x1C */
    RtAnimInterpolator* currentAnim;    /* 0x20 */
}; /* size 0x24 */

/* The plugin's extension of a frame: the ID of its node and the hierarchy it roots. */
typedef struct RpHAnimFrameExtension {
    RwInt32 id;                 /* 0x0: -1 when the frame is no node */
    RpHAnimHierarchy* hierarchy;/* 0x4 */
} RpHAnimFrameExtension; /* size 0x8 */

/* The plugin's globals: where the frame extension lies in a frame, and the hierarchy free list. */
typedef struct RpHAnimAtomicGlobalVars {
    RwInt32 engineOffset;       /* 0x0 */
    RwFreeList* HAnimFreeList;  /* 0x4 */
} RpHAnimAtomicGlobalVars;

extern RpHAnimAtomicGlobalVars RpHAnimAtomicGlobals;

/*
 * Sets a matrix from an interpolated standard keyframe: its rotation and its translation. The
 * quaternions of RpHAnim are RtQuat's, so no conjugate is needed (RpSkin's differ).
 */
#define RpHAnimKeyFrameToMatrixMacro(_matrix, _voidIFrame)                  \
    do {                                                                    \
        RpHAnimInterpFrame* iFrame = (RpHAnimInterpFrame*)(_voidIFrame);    \
                                                                            \
        RtQuatUnitConvertToMatrix(&iFrame->q, (_matrix));                   \
                                                                            \
        (_matrix)->pos.x = iFrame->t.x;                                     \
        (_matrix)->pos.y = iFrame->t.y;                                     \
        (_matrix)->pos.z = iFrame->t.z;                                     \
    } while (0)

/* The hierarchy plugin's functions. */
RwBool RpHAnimPluginAttach(void);
RpHAnimHierarchy* RpHAnimHierarchyCreate(RwInt32 numNodes, RwUInt32* nodeFlags, RwInt32* nodeIDs,
                                         RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize);
RpHAnimHierarchy* RpHAnimHierarchyDestroy(RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimHierarchyCreateSubHierarchy(RpHAnimHierarchy* parentHierarchy, RwInt32 startNode,
                                                     RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize);
RpHAnimHierarchy* RpHAnimHierarchyAttach(RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimFrameGetHierarchy(RwFrame* frame);
RwInt32 RpHAnimFrameGetID(RwFrame* frame);
RwMatrix* RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy* hierarchy);
RwBool RpHAnimHierarchyUpdateMatrices(RpHAnimHierarchy* hierarchy);
RwInt32 RpHAnimIDGetIndex(RpHAnimHierarchy* hierarchy, RwInt32 ID);

/* The standard keyframe scheme's callbacks (stdkey.c). */
void RpHAnimKeyFrameApply(void* pMatrix, void* pVoidIFrame);
void RpHAnimKeyFrameInterpolate(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal time, void* customData);
void RpHAnimKeyFrameBlend(void* pVoidOut, void* pVoidIn1, void* pVoidIn2, RwReal alpha);
RtAnimAnimation* RpHAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* pAnimation);
RwBool RpHAnimKeyFrameStreamWrite(const RtAnimAnimation* pAnimation, RwStream* stream);
RwInt32 RpHAnimKeyFrameStreamGetSize(const RtAnimAnimation* pAnimation);
void RpHAnimKeyFrameMulRecip(void* pVoidFrame, void* pVoidStart);
void RpHAnimKeyFrameAdd(void* pVoidOut, void* pVoidIn1, void* pVoidIn2);

#ifdef __cplusplus
}
#endif

#endif
