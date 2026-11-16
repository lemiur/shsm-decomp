// Reference: bfbbdecomp/bfbb include/rwsdk/rpworld.h
// Reference: tgsm/pbg include/rwsdk/world/baclump.h
#ifndef RWSDK_WORLD_BACLUMP_H
#define RWSDK_WORLD_BACLUMP_H

#include <rwsdk/bacamera.h>
#include <rwsdk/baframe.h>
#include <rwsdk/batypehf.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/balight.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RpInterpolatorFlag: what an atomic's interpolator has changed since the atomic was last used. */
typedef enum RpInterpolatorFlag {
    rpINTERPOLATORDIRTYINSTANCE = 0x01,
    rpINTERPOLATORDIRTYSPHERE = 0x02,
    rpINTERPOLATORNOFRAMEDIRTY = 0x04,
    rpINTERPOLATORFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RpInterpolatorFlag;

/* The object types of an atomic and a clump (bfbbdecomp/bfbb rpworld.h). */
#define rpATOMIC 1
#define rpCLUMP 2

/* RpAtomicFlag: kept in an atomic's object flags. */
typedef enum RpAtomicFlag {
    rpATOMICCOLLISIONTEST = 0x01,
    rpATOMICRENDER = 0x04,
    rpATOMICFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} RpAtomicFlag;

/* RpAtomicPrivateFlag: kept in an atomic's private object flags. */
typedef enum RpAtomicPrivateFlag {
    rpATOMICPRIVATEWORLDBOUNDDIRTY = 0x01, /* the world bounding sphere must be recomputed */
    rpATOMICPRIVATEFLAGFORCEENUMSIZEINT = 0x7FFFFFFF
} rpAtomicPrivateFlag;

/* rpClumpGlobals: the clump module's globals, the free lists of atomics and clumps. */
typedef struct rpClumpGlobals {
    RwFreeList* atomicFreeList; /* 0x0 */
    RwFreeList* clumpFreeList;  /* 0x4 */
} rpClumpGlobals;

typedef struct RpClump RpClump;

/* A clump's callback (RpClumpSetCallBack), and the callback of an iteration over clumps. */
typedef RpClump* (*RpClumpCallBack)(RpClump* clump, void* data);

/*
 * RpClump: a group of atomics, lights and cameras sharing a frame hierarchy, and its link in its
 * world's clump list. Size 0x2C, from retail's clump plugin registry.
 */
struct RpClump {
    RwObject object;            /* 0x0 */
    RwLinkList atomicList;      /* 0x8 */
    RwLinkList lightList;       /* 0x10 */
    RwLinkList cameraList;      /* 0x18 */
    RwLLLink inWorldLink;       /* 0x20 */
    RpClumpCallBack callback;   /* 0x28 */
};

/* RpClumpChunkInfo: a clump's structure chunk: how many atomics, lights and cameras follow. */
typedef struct RpClumpChunkInfo {
    RwInt32 numAtomics; /* 0x0 */
    RwInt32 numLights;  /* 0x4 */
    RwInt32 numCameras; /* 0x8 */
} RpClumpChunkInfo;

/* RpInterpolator: which two morph targets an atomic's geometry is between, and how far. */
typedef struct RpInterpolator {
    RwInt32 flags;              /* 0x0 */
    RwInt16 startMorphTarget;   /* 0x4 */
    RwInt16 endMorphTarget;     /* 0x6 */
    RwReal time;                /* 0x8 */
    RwReal recipTime;           /* 0xC */
    RwReal position;            /* 0x10 */
} RpInterpolator;

typedef struct RpAtomic RpAtomic;

/* Renders an atomic; NULL reports a failure. */
typedef RpAtomic* (*RpAtomicCallBackRender)(RpAtomic* atomic);

/*
 * RpAtomic: a geometry placed in the world through a frame: its instanced data, its bounding
 * spheres (local and world), the clump it belongs to, how it renders and which morph targets it
 * shows, and the world sectors it lies in. Size 0x70, from retail's atomic plugin registry.
 */
struct RpAtomic {
    RwObjectHasFrame object;            /* 0x0 */
    RwResEntry* repEntry;               /* 0x14 */
    RpGeometry* geometry;               /* 0x18 */
    RwSphere boundingSphere;            /* 0x1C */
    RwSphere worldBoundingSphere;       /* 0x2C */
    RpClump* clump;                     /* 0x3C */
    RwLLLink inClumpLink;               /* 0x40 */
    RpAtomicCallBackRender renderCallBack; /* 0x48 */
    RpInterpolator interpolator;        /* 0x4C */
    RwUInt16 renderFrame;               /* 0x60 */
    /* 0x101 when created, copied by a clone. */
    RwUInt16 pad;                       /* 0x62 */ // pad: name as in the RenderWare SDK
    RwLinkList llWorldSectorsInAtomic;  /* 0x64 */
    RxPipeline* pipeline;               /* 0x6C */
};

/* Called for each atomic of an iteration; NULL stops it. */
typedef RpAtomic* (*RpAtomicCallBack)(RpAtomic* atomic, void* data);

/* Atomic and clump accessors (bfbbdecomp/bfbb rpworld.h, the SDK's release macros). */
#define RpAtomicRender(_atomic) ((_atomic)->renderCallBack(_atomic))
#define RpAtomicGetGeometry(_atomic) ((_atomic)->geometry)
#define RpAtomicGetRenderCallBack(_atomic) ((_atomic)->renderCallBack)
#define RpAtomicGetInterpolator(_atomic) (&((_atomic)->interpolator))
#define RpAtomicGetClump(_atomic) ((_atomic)->clump)
#define RpAtomicGetFrame(_atomic) ((RwFrame*)rwObjectGetParent(_atomic))
#define RpAtomicGetFlags(_atomic) (rwObjectGetFlags(_atomic))
#define RpAtomicGetBoundingSphere(_atomic)                                                ((((_atomic)->interpolator.flags & rpINTERPOLATORDIRTYSPHERE) ?                             _rpAtomicResyncInterpolatedSphere(_atomic), 0 : 0),                              &((_atomic)->boundingSphere))
#define RpClumpSetFrame(_clump, _frame) (rwObjectSetParent(_clump, _frame), (_clump))
#define RpClumpGetFrame(_clump) ((RwFrame*)rwObjectGetParent(_clump))
#define RpClumpSetCallBack(_clump, _callback) ((_clump)->callback = (_callback))
#define RpAtomicSetRenderCallBack(_atomic, _callback)                    do {                                                                     (_atomic)->renderCallBack = (_callback);                             if (!(_atomic)->renderCallBack) {                                        (_atomic)->renderCallBack = AtomicDefaultRenderCallBack;         }                                                                } while (0)

/* The atomic rights stream callbacks (rwID_RIGHTTORENDER), registered by _rpWorldObjRegisterExtensions. */
RwStream* _rpReadAtomicRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                              RwInt32 sizeInObject);
RwStream* _rpWriteAtomicRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                               RwInt32 offsetInObject, RwInt32 sizeInObject);
RwInt32 _rpSizeAtomicRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

RpAtomic* AtomicDefaultRenderCallBack(RpAtomic* atomic);
void _rpAtomicResyncInterpolatedSphere(RpAtomic* atomic);
const RwSphere* RpAtomicGetWorldBoundingSphere(RpAtomic* atomic);
void* _rpClumpClose(void* instance, RwInt32 offset, RwInt32 size);
void RpAtomicSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc);
void* _rpClumpOpen(void* instance, RwInt32 offset, RwInt32 size);
RwBool _rpClumpRegisterExtensions(void);
RpClump* RpClumpRender(RpClump* clump);
RpClump* RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack callback, void* pData);
RpClump* RpClumpForAllCameras(RpClump* clump, RwCameraCallBack callback, void* pData);
RpClump* RpClumpForAllLights(RpClump* clump, RpLightCallBack callback, void* pData);
RpAtomic* RpAtomicCreate(void);
RwBool RpAtomicDestroy(RpAtomic* atomic);
RpAtomic* RpAtomicClone(RpAtomic* atomic);
RpClump* RpClumpClone(RpClump* clump);
RpClump* RpClumpCreate(void);
RwBool RpClumpDestroy(RpClump* clump);
RpClump* RpClumpAddAtomic(RpClump* clump, RpAtomic* atomic);
RpAtomic* RpAtomicSetGeometry(RpAtomic* atomic, RpGeometry* geometry, RwUInt32 flags);
RpClump* RpClumpAddLight(RpClump* clump, RpLight* light);
RpClump* RpClumpAddCamera(RpClump* clump, RwCamera* camera);
RpClump* RpClumpRemoveLight(RpClump* clump, RpLight* light);
RpClump* RpClumpRemoveCamera(RpClump* clump, RwCamera* camera);
RpAtomic* RpAtomicStreamRead(RwStream* stream);
RpClump* RpClumpStreamRead(RwStream* stream);
RwInt32 RpAtomicRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                               RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RpClumpRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwInt32 RpAtomicRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                     RwPluginDataChunkWriteCallBack writeCB,
                                     RwPluginDataChunkGetSizeCallBack getSizeCB);
RwInt32 RpAtomicSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB);
RwInt32 RpAtomicSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB);
RwInt32 RpAtomicGetPluginOffset(RwUInt32 pluginID);
RpAtomic* RpAtomicSetFrame(RpAtomic* atomic, RwFrame* frame);

#ifdef __cplusplus
}
#endif

#endif
