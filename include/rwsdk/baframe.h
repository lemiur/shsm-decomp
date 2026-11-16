// Reference: tgsm/pbg include/rwsdk/baframe.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BAFRAME_H
#define RWSDK_BAFRAME_H

#include <rwsdk/batypehf.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/batkreg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The object type of a frame. */
#define rwFRAME 0

/*
 * A frame's private flags. A hierarchy whose root has a HIERARCHYSYNC flag set sits in the dirty
 * frame list (RwGlobals dirtyFrameList); a frame with a SUBTREESYNC flag set has moved since the
 * last synchronisation. Member names and values follow the RenderWare SDK,
 * where they are #defines; the enum name is invented.
 */
typedef enum RwFramePrivateFlag {
    rwFRAMEPRIVATEHIERARCHYSYNCLTM = 0x01,  /* the hierarchy has LTMs to rebuild */
    rwFRAMEPRIVATEHIERARCHYSYNCOBJ = 0x02,  /* the hierarchy has attached objects to resync */
    rwFRAMEPRIVATEHIERARCHYSYNC = rwFRAMEPRIVATEHIERARCHYSYNCLTM | rwFRAMEPRIVATEHIERARCHYSYNCOBJ,
    rwFRAMEPRIVATESUBTREESYNCLTM = 0x04,    /* this frame's LTM is out of date */
    rwFRAMEPRIVATESUBTREESYNCOBJ = 0x08,    /* this frame's objects are out of date */
    rwFRAMEPRIVATESUBTREESYNC = rwFRAMEPRIVATESUBTREESYNCLTM | rwFRAMEPRIVATESUBTREESYNCOBJ,
    rwFRAMEPRIVATEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RwFramePrivateFlag;

/*
 * RwFrame: a node of a frame hierarchy: its transformation relative to its parent (the object's
 * parent), its local transformation matrix (LTM), the objects attached to it, its first child,
 * its next sibling and the hierarchy's root.
 */
struct RwFrame {
    RwObject object;            /* 0x0 */
    RwLLLink inDirtyListLink;   /* 0x8: in RwGlobals dirtyFrameList (a root only) */
    RwMatrix modelling;         /* 0x10 */
    RwMatrix ltm;               /* 0x50 */
    RwLinkList objectList;      /* 0x90: the RwObjectHasFrames attached */
    struct RwFrame* child;      /* 0x98 */
    struct RwFrame* next;       /* 0x9C */
    struct RwFrame* root;       /* 0xA0 */
};

/* The SDK declares the typedef with the frame alignment. */
typedef struct RwFrame RWALIGN(RwFrame, rwFRAMEALIGNMENT);

typedef RwFrame* (*RwFrameCallBack)(RwFrame* frame, void* data);

#define RwFrameGetParent(_f) ((RwFrame*)rwObjectGetParent(_f))
#define RwFrameGetMatrix(_f) (&(_f)->modelling)

/* The plugins registered with frames. */
extern RwPluginRegistry frameTKList;

void RwFrameSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc);
void* _rwFrameOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwFrameClose(void* instance, RwInt32 offset, RwInt32 size);
RwFrame* _rwFrameCloneAndLinkClones(RwFrame* root);
RwFrame* _rwFramePurgeClone(RwFrame* root);
RwBool RwFrameDirty(const RwFrame* frame);
void _rwFrameInit(RwFrame* frame);
RwFrame* RwFrameCreate(void);
void _rwFrameDeInit(RwFrame* frame);
RwBool RwFrameDestroy(RwFrame* frame);
RwBool RwFrameDestroyHierarchy(RwFrame* frame);
RwFrame* RwFrameUpdateObjects(RwFrame* frame);
RwMatrix* RwFrameGetLTM(RwFrame* frame);
RwFrame* RwFrameGetRoot(const RwFrame* frame);
RwFrame* RwFrameAddChildNoUpdate(RwFrame* parent, RwFrame* child);
RwFrame* RwFrameAddChild(RwFrame* parent, RwFrame* child);
RwFrame* RwFrameRemoveChild(RwFrame* child);
RwFrame* RwFrameForAllChildren(RwFrame* frame, RwFrameCallBack callBack, void* data);
RwFrame* RwFrameTranslate(RwFrame* frame, const RwV3d* v, RwOpCombineType combine);
RwFrame* RwFrameTransform(RwFrame* frame, const RwMatrix* m, RwOpCombineType combine);
RwFrame* RwFrameRotate(RwFrame* frame, const RwV3d* axis, RwReal angle, RwOpCombineType combine);
RwFrame* RwFrameSetIdentity(RwFrame* frame);
RwInt32 RwFrameRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RwFrameGetPluginOffset(RwUInt32 pluginID);

/*
 * Frame synchronisation, defined in basync.c.
 */
/* Brings every dirty hierarchy up to date. */
RwBool _rwFrameSyncDirty(void);
/* Rebuilds the LTMs of a hierarchy, leaving its objects dirty. */
void _rwFrameSyncHierarchyLTM(RwFrame* frame);

#ifdef __cplusplus
}
#endif

#endif
