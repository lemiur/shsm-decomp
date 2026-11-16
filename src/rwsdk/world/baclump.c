// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt (function names), include/rwsdk/rpworld.h
// Reference: tgsm/pbg config/GPLE9G/symbols.txt (function and data names)
/*
 * baclump.c: RenderWare atomics and clumps. An atomic places a geometry in the world through a
 * frame and renders it through a pipeline; a clump groups atomics, lights and cameras under one
 * frame hierarchy and is the unit a model is streamed in. The clump module's two free lists hand
 * out the atomics and the clumps.
 * Frame pointers are tested for truth (`if (frame)`), never compared with NULL: RwFrame is an
 * aligned type, and MWCC skips the IR optimiser in any function that compares a pointer to an
 * aligned type or loads a member through one (docs/compiler_regalloc.md, "IR optimiser gate").
 */
#include <rwsdk/world/rpplugin.h>
#include <rwsdk/babincam.h>
#include <rwsdk/babinfrm.h>
#include <rwsdk/bacamera.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/balight.h>
#include <rwsdk/world/baworobj.h>
#include <string.h>

/* The free lists of atomics and clumps; RwFreeListCreateAndPreallocateSpace builds them in place here. */
static RwFreeList _rpAtomicFreeList;
static RwFreeList _rpClumpFreeList;

/* The plugins registered with atomics: no plugin data yet, so an atomic is just an RpAtomic. */
RwPluginRegistry atomicTKList = {
    sizeof(RpAtomic),
    sizeof(RpAtomic),
    0,
    0,
    NULL,
    NULL,
};

/* The plugins registered with clumps: no plugin data yet, so a clump is just an RpClump. */
RwPluginRegistry clumpTKList = {
    sizeof(RpClump),
    sizeof(RpClump),
    0,
    0,
    NULL,
    NULL,
};

/* Atomics and clumps per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rpAtomicFreeListBlockSize = 128;
static RwInt32 _rpAtomicFreeListPreallocBlocks = 1;
static RwInt32 _rpClumpFreeListBlockSize = 128;
static RwInt32 _rpClumpFreeListPreallocBlocks = 1;

/*
 * The module's uninitialised small data, declared in reverse of its retail order (the compiler lays
 * such statics out last declared first).
 */

/* The clump module's registration (offset of its globals in the engine instance). */
static RwModuleInfo clumpModule;

/* The rights plugin and its data last read with an atomic (bfbb names). */
static RwInt32 lastSeenRightsPluginId;
static RwInt32 lastSeenExtraData;

/*
 * The atomic AtomicDefaultRenderCallBack is running its pipeline over, NULL between renders.
 * Retail only (bfbbdecomp/bfbb and tgsm/pbg have no such variable); nothing in retail reads it.
 */
RpAtomic* lbl_8052B408;

/* Where the clump's extension sits in each light and each camera. */
static RwInt32 _rpClumpLightExtOffset;
static RwInt32 _rpClumpCameraExtOffset;

/* The clump module's globals (rpClumpGlobals, baclump.h). Macro name invented, after bacamera's. */
#define RWCLUMPGLOBAL(var) \
    (RWPLUGINOFFSET(rpClumpGlobals, RwEngineInstance, clumpModule.globalsOffset)->var)

/*
 * The clump's extension of a light or a camera: the clump it belongs to and its link in that
 * clump's light or camera list. Names invented (the type and the two macros).
 */
typedef struct rpClumpObjectExt {
    RpClump* clump;         /* 0x0 */
    RwLLLink inClumpLink;   /* 0x4 */
} rpClumpObjectExt;

#define CLUMPLIGHTEXTFROMLIGHT(light) \
    (RWPLUGINOFFSET(rpClumpObjectExt, light, _rpClumpLightExtOffset))
#define CLUMPCAMERAEXTFROMCAMERA(camera) \
    (RWPLUGINOFFSET(rpClumpObjectExt, camera, _rpClumpCameraExtOffset))

/*
 * What RpClumpClone passes to ClumpAtomicCloneAndAttach: the clump being cloned, its clone, and
 * whether every atomic was cloned. Names invented.
 */
typedef struct rpClumpCloneData {
    RpClump* clump;         /* 0x0 */
    RpClump* clumpClone;    /* 0x4 */
    RwBool success;         /* 0x8 */
} rpClumpCloneData;

/* The geometries of a clump's geometry list chunk, which its atomics name by index. Names invented. */
typedef struct rpGeometryList {
    RpGeometry** geometries;    /* 0x0 */
    RwInt32 numGeoms;           /* 0x4 */
} rpGeometryList;

/*
 * An atomic's structure chunk: the indices of its frame and geometry in the clump's lists, and its
 * flags. Names invented, after aap/librw's atomic chunk reader.
 */
typedef struct rpAtomicBinary {
    RwInt32 frameIndex; /* 0x0 */
    RwInt32 geomIndex;  /* 0x4 */
    RwInt32 flags;      /* 0x8 */
    RwInt32 unused;     /* 0xC */
} rpAtomicBinary;

/* The larger of two or three reals. Names invented. */
#define RwRealMax2(a, b) ((a) >= (b) ? (a) : (b))
#define RwRealMax3(a, b, c) RwRealMax2(a, RwRealMax2(b, c))

/* Destroys a clump still in use when the clump module closes. */
static void ClumpTidyDestroyClump(void* clump, void* data) {
    RpClumpDestroy((RpClump*)clump);
} // 0x802CEE4C

/* Destroys an atomic still in use when the clump module closes. */
static void ClumpTidyDestroyAtomic(void* atomic, void* data) {
    RpAtomicDestroy((RpAtomic*)atomic);
} // 0x802CEF5C

/* Reads the plugin owning an atomic's pipeline, and its data when the chunk has it. */
RwStream* _rpReadAtomicRights(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject,
                              RwInt32 sizeInObject) {
    if (!RwStreamReadInt32(stream, &lastSeenRightsPluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (binaryLength == 2 * (RwInt32)sizeof(RwInt32)) {
        if (!RwStreamReadInt32(stream, &lastSeenExtraData, sizeof(RwInt32))) {
            return NULL;
        }
    }

    return stream;
} // 0x802CF010

/* Writes the plugin owning an atomic's pipeline and its data. */
RwStream* _rpWriteAtomicRights(RwStream* stream, RwInt32 binaryLength, const void* object,
                               RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpAtomic* atomic = (const RpAtomic*)object;

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&atomic->pipeline->pluginId, sizeof(RwInt32))) {
        return NULL;
    }

    if (RwStreamWriteInt32(stream, (const RwInt32*)&atomic->pipeline->pluginData, sizeof(RwInt32))) {
        return stream;
    }

    return NULL;
} // 0x802CF08C

/* The size of an atomic's rights chunk: written only when a plugin owns its pipeline. */
RwInt32 _rpSizeAtomicRights(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const RpAtomic* atomic = (const RpAtomic*)object;

    if (atomic->pipeline != NULL && atomic->pipeline->pluginId != 0) {
        return 2 * sizeof(RwInt32);
    }

    return 0;
} // 0x802CF108

/*
 * An atomic's frame has moved: its bounding sphere is brought up to date with its morph targets
 * and its world bounding sphere marked stale.
 */
static RwObjectHasFrame* AtomicSync(RwObjectHasFrame* object) {
    RpAtomic* atomic = (RpAtomic*)object;

    if (atomic->interpolator.flags & rpINTERPOLATORDIRTYSPHERE) {
        _rpAtomicResyncInterpolatedSphere(atomic);
    }

    rwObjectSetPrivateFlags(atomic, rwObjectGetPrivateFlags(atomic) | rpATOMICPRIVATEWORLDBOUNDDIRTY);

    return object;
} // 0x802CF130

/* Renders an atomic through its pipeline, or through the default atomic pipeline if it has none. */
RpAtomic* AtomicDefaultRenderCallBack(RpAtomic* atomic) {
    RxPipeline* pipeline = atomic->pipeline;

    if (pipeline == NULL) {
        pipeline = RXPIPELINEGLOBAL(currentAtomicPipeline);
    }

    lbl_8052B408 = atomic;
    if (RxPipelineExecute(pipeline, atomic, TRUE)) {
        lbl_8052B408 = NULL;
        return atomic;
    }

    lbl_8052B408 = NULL;
    return NULL;
} // 0x802CF178

/*
 * Clones an atomic of the clump being cloned into the clump's clone, on the clone of its frame
 * (which _rwFrameCloneAndLinkClones left in the frame's root). Reading frame->root loads a member
 * through the aligned RwFrame, so the IR optimiser skips this function: the RpAtomicDestroy inlined
 * on the no-frame path keeps its RpAtomicSetGeometry(atomic, NULL, 0) unfolded, as in retail.
 */
static RpAtomic* ClumpAtomicCloneAndAttach(RpAtomic* atomic, void* data) {
    rpClumpCloneData* cloneData = (rpClumpCloneData*)data;
    RpAtomic* newAtomic;
    RwFrame* frame;

    newAtomic = RpAtomicClone(atomic);
    if (newAtomic == NULL) {
        cloneData->success = FALSE;
        return NULL;
    }

    frame = RpAtomicGetFrame(atomic);
    if (frame) {
        RpAtomicSetFrame(newAtomic, frame->root);
    } else {
        RpAtomicDestroy(newAtomic);
        RWERROR((E_RP_WORLD_ATOMICNOFRAME));
        return NULL;
    }

    RpClumpAddAtomic(cloneData->clumpClone, newAtomic);

    return atomic;
} // 0x802CF1F4

/* Destroys the geometries read so far and the list's array. Name invented. */
static rpGeometryList* GeometryListDeinitialize(rpGeometryList* geomList) {
    RwInt32 i;

    for (i = 0; i < geomList->numGeoms; i++) {
        RpGeometryDestroy(geomList->geometries[i]);
    }

    if (geomList->geometries != NULL) {
        RwFree(geomList->geometries);
        geomList->geometries = NULL;
    }

    return geomList;
} // no copy in retail (inlined)

/* Reads a clump's geometry list chunk: the count, then each geometry's chunk. */
static rpGeometryList* GeometryListStreamRead(RwStream* stream, rpGeometryList* geomList) {
    RwInt32 numGeoms;
    RwUInt32 size;
    RwUInt32 version;
    RwInt32 i;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        if (RwStreamRead(stream, &numGeoms, sizeof(numGeoms)) != sizeof(numGeoms)) {
            return NULL;
        }
        RwMemNative32(&numGeoms, sizeof(numGeoms));

        geomList->numGeoms = 0;
        if (numGeoms > 0) {
            geomList->geometries =
                (RpGeometry**)RwMalloc(numGeoms * sizeof(RpGeometry*), rwMEMHINTDUR_EVENT | rwID_GEOMETRY);
            if (geomList->geometries == NULL) {
                RWERROR((E_RW_NOMEM, numGeoms * sizeof(RpGeometry*)));
                return NULL;
            }
        } else {
            geomList->geometries = NULL;
        }

        for (i = 0; i < numGeoms; i++) {
            if (!RwStreamFindChunk(stream, rwID_GEOMETRY, NULL, &version)) {
                GeometryListDeinitialize(geomList);
                return NULL;
            }

            if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
                if ((geomList->geometries[i] = RpGeometryStreamRead(stream)) == NULL) {
                    GeometryListDeinitialize(geomList);
                    return NULL;
                }
                geomList->numGeoms++;
            } else {
                GeometryListDeinitialize(geomList);
                return NULL;
            }
        }
    } else {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    return geomList;
} // 0x802CF37C

/* A camera's clump extension starts out in no clump. */
static void* ClumpInitCameraExt(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    rpClumpObjectExt* ext = CLUMPCAMERAEXTFROMCAMERA(object);

    rwLLLinkInitialize(&ext->inClumpLink);
    ext->clump = NULL;

    return object;
} // 0x802CF648

/* A camera's clump extension owns nothing. */
static void* ClumpDeInitCameraExt(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return object;
} // 0x802CF664

/* A light's clump extension starts out in no clump. */
static void* ClumpInitLightExt(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    rpClumpObjectExt* ext = CLUMPLIGHTEXTFROMLIGHT(object);

    rwLLLinkInitialize(&ext->inClumpLink);
    ext->clump = NULL;

    return object;
} // 0x802CF668

/* A light's clump extension owns nothing. */
static void* ClumpDeInitLightExt(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return object;
} // 0x802CF684

/* Takes a light out of the clump being destroyed, and destroys it. Name invented. */
static RpLight* ClumpDestroyLight(RpLight* light, void* data) {
    RpClumpRemoveLight(CLUMPLIGHTEXTFROMLIGHT(light)->clump, light);
    RpLightDestroy(light);

    return light;
} // 0x802CF688

/* Takes a camera out of the clump being destroyed, and destroys it. Name invented. */
static RwCamera* ClumpDestroyCamera(RwCamera* camera, void* data) {
    RpClumpRemoveCamera(CLUMPCAMERAEXTFROMCAMERA(camera)->clump, camera);
    RwCameraDestroy(camera);

    return camera;
} // 0x802CF6E8

/* A new clump's callback: does nothing. */
static RpClump* ClumpCallBack(RpClump* clump, void* data) {
    return clump;
} // 0x802CF748

/* Destroys an atomic of the clump being destroyed. Name invented. */
static RpAtomic* ClumpDestroyAtomic(RpAtomic* atomic, void* data) {
    RpAtomicDestroy(atomic);

    return atomic;
} // 0x802CF74C

/*
 * Reads an atomic's chunk: its flags, its frame and geometry (named by index in the clump's lists,
 * or the geometry in a chunk of its own when the clump has none), its plugin data and its rights.
 */
static RpAtomic* ClumpAtomicStreamRead(RwStream* stream, rwFrameList* frameList, rpGeometryList* geomList) {
    RwUInt32 size;
    RwUInt32 version;
    rpAtomicBinary atomicData;
    RpAtomic* atomic;
    RpGeometry* geometry;
    RwBool status;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)) {
        RWERROR((E_RW_READ));
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        memset(&atomicData, 0, sizeof(atomicData));
        status = (RwStreamRead(stream, &atomicData, size) == size);
        if (!status) {
            RWERROR((E_RW_READ));
            return NULL;
        }
        RwMemNative32(&atomicData, sizeof(atomicData));

        atomic = RpAtomicCreate();
        if (atomic == NULL) {
            return NULL;
        }

        rwObjectSetFlags(atomic, atomicData.flags);

        if (frameList->numFrames != 0) {
            RpAtomicSetFrame(atomic, frameList->frames[atomicData.frameIndex]);
        }

        if (geomList->numGeoms != 0) {
            RpAtomicSetGeometry(atomic, geomList->geometries[atomicData.geomIndex], 0);
        } else {
            if (!RwStreamFindChunk(stream, rwID_GEOMETRY, NULL, &version)) {
                RpAtomicDestroy(atomic);
                RWERROR((E_RW_READ));
                return NULL;
            }

            if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
                geometry = RpGeometryStreamRead(stream);
                status = (geometry != NULL);
                if (!status) {
                    RpAtomicDestroy(atomic);
                    RWERROR((E_RW_READ));
                    return NULL;
                }
            } else {
                RpAtomicDestroy(atomic);
                RWERROR((E_RW_BADVERSION));
                return NULL;
            }

            RpAtomicSetGeometry(atomic, geometry, 0);
            RpGeometryDestroy(geometry);
        }

        lastSeenRightsPluginId = 0;
        lastSeenExtraData = 0;
        status = (_rwPluginRegistryReadDataChunks(&atomicTKList, stream, atomic) != NULL);
        if (!status) {
            RWERROR((E_RW_READ));
            return NULL;
        }

        if (lastSeenRightsPluginId != 0) {
            _rwPluginRegistryInvokeRights(&atomicTKList, lastSeenRightsPluginId, atomic, lastSeenExtraData);
        }

        return atomic;
    } else {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }
} // 0x802CF804

/*
 * Brings an atomic's bounding sphere up to date with its interpolator: between the start and end
 * morph targets' spheres, the start target's when both are the same, the first target's when the
 * interpolator names a target the geometry does not have.
 */
void _rpAtomicResyncInterpolatedSphere(RpAtomic* atomic) {
    RpGeometry* geometry = atomic->geometry;
    RpInterpolator* interpolator = &atomic->interpolator;

    if (geometry == NULL) {
        return;
    }

    if (interpolator->startMorphTarget == interpolator->endMorphTarget ||
        interpolator->startMorphTarget >= geometry->numMorphTargets ||
        interpolator->endMorphTarget >= geometry->numMorphTargets) {
        if (interpolator->startMorphTarget >= geometry->numMorphTargets ||
            interpolator->endMorphTarget >= geometry->numMorphTargets) {
            atomic->boundingSphere = geometry->morphTarget[0].boundingSphere;
        } else {
            atomic->boundingSphere = geometry->morphTarget[interpolator->startMorphTarget].boundingSphere;
        }
    } else {
        const RwSphere* startSphere = &geometry->morphTarget[interpolator->startMorphTarget].boundingSphere;
        const RwSphere* endSphere = &geometry->morphTarget[interpolator->endMorphTarget].boundingSphere;
        RwReal scale = interpolator->recipTime * interpolator->position;

        atomic->boundingSphere.radius = startSphere->radius + scale * (endSphere->radius - startSphere->radius);
        RwV3dSubMacro(&atomic->boundingSphere.center, &endSphere->center, &startSphere->center);
        RwV3dScaleMacro(&atomic->boundingSphere.center, &atomic->boundingSphere.center, scale);
        RwV3dAddMacro(&atomic->boundingSphere.center, &atomic->boundingSphere.center, &startSphere->center);
    }

    interpolator->flags &= ~rpINTERPOLATORDIRTYSPHERE;
    rwObjectSetPrivateFlags(atomic, rwObjectGetPrivateFlags(atomic) | rpATOMICPRIVATEWORLDBOUNDDIRTY);
} // 0x802CFD84

/*
 * An atomic's bounding sphere in world space, recomputed when its frame or its bounding sphere has
 * changed: the centre goes through the frame's LTM and the radius grows by the LTM's largest axis
 * scale (unless the LTM is orthonormal).
 */
const RwSphere* RpAtomicGetWorldBoundingSphere(RpAtomic* atomic) {
    RwFrame* frame = RpAtomicGetFrame(atomic);

    RpAtomicGetBoundingSphere(atomic);

    if (RwFrameDirty(frame) || rwObjectTestPrivateFlags(atomic, rpATOMICPRIVATEWORLDBOUNDDIRTY)) {
        RwMatrix* ltm = RwFrameGetLTM(frame);

        RwV3dTransformPoint(&atomic->worldBoundingSphere.center, &atomic->boundingSphere.center, ltm);

        if ((rwMatrixGetFlags(ltm) & rwMATRIXTYPEMASK) != rwMATRIXTYPEORTHONORMAL) {
            RwReal rightScaleSq = RwV3dDotProductMacro(&ltm->right, &ltm->right);
            RwReal upScaleSq = RwV3dDotProductMacro(&ltm->up, &ltm->up);
            RwReal atScaleSq = RwV3dDotProductMacro(&ltm->at, &ltm->at);

            atomic->worldBoundingSphere.radius =
                atomic->boundingSphere.radius * _rwSqrt(RwRealMax3(rightScaleSq, upScaleSq, atScaleSq));
        } else {
            atomic->worldBoundingSphere.radius = atomic->boundingSphere.radius;
        }

        rwObjectSetPrivateFlags(atomic, rwObjectGetPrivateFlags(atomic) & ~rpATOMICPRIVATEWORLDBOUNDDIRTY);
    }

    return &atomic->worldBoundingSphere;
} // 0x802CFEDC

/* Closes the clump module: destroys the clumps and atomics still in use, then their free lists. */
void* _rpClumpClose(void* instance, RwInt32 offset, RwInt32 size) {
    RwFreeListForAllUsed(RWCLUMPGLOBAL(clumpFreeList), ClumpTidyDestroyClump, NULL);
    RwFreeListForAllUsed(RWCLUMPGLOBAL(atomicFreeList), ClumpTidyDestroyAtomic, NULL);

    RwFreeListDestroy(RWCLUMPGLOBAL(atomicFreeList));
    RwFreeListDestroy(RWCLUMPGLOBAL(clumpFreeList));
    RWCLUMPGLOBAL(atomicFreeList) = NULL;
    RWCLUMPGLOBAL(clumpFreeList) = NULL;

    clumpModule.numInstances--;

    return instance;
} // 0x802D0034

/* Sets the size of the atomics' free-list blocks and how many the module allocates when it opens. */
void RpAtomicSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc) {
    _rpAtomicFreeListBlockSize = blockSize;
    _rpAtomicFreeListPreallocBlocks = numBlocksToPrealloc;
} // 0x802D00F0

/*
 * Opens the clump module: creates the atomics' and the clumps' free lists. The atomics' list is
 * created with the clump's memory hint and the clumps' with the atomic's (the original's swap).
 */
void* _rpClumpOpen(void* instance, RwInt32 offset, RwInt32 size) {
    clumpModule.globalsOffset = offset;

    RWCLUMPGLOBAL(atomicFreeList) = RwFreeListCreateAndPreallocateSpace(
        atomicTKList.sizeOfStruct, _rpAtomicFreeListBlockSize, sizeof(RwUInt32),
        _rpAtomicFreeListPreallocBlocks, &_rpAtomicFreeList, rwMEMHINTDUR_GLOBAL | rwID_CLUMP);
    if (RWCLUMPGLOBAL(atomicFreeList) != NULL) {
        RWCLUMPGLOBAL(clumpFreeList) = RwFreeListCreateAndPreallocateSpace(
            clumpTKList.sizeOfStruct, _rpClumpFreeListBlockSize, sizeof(RwUInt32),
            _rpClumpFreeListPreallocBlocks, &_rpClumpFreeList, rwMEMHINTDUR_GLOBAL | rwID_ATOMIC);
        if (RWCLUMPGLOBAL(clumpFreeList) != NULL) {
            clumpModule.numInstances++;
            return instance;
        }

        RwFreeListDestroy(RWCLUMPGLOBAL(atomicFreeList));
        RWCLUMPGLOBAL(atomicFreeList) = NULL;
    }

    return NULL;
} // 0x802D00FC

/* Registers the clump's extensions of cameras and lights. */
RwBool _rpClumpRegisterExtensions(void) {
    _rpClumpCameraExtOffset = RwCameraRegisterPlugin(sizeof(rpClumpObjectExt), rwID_CLUMP, ClumpInitCameraExt,
                                                     ClumpDeInitCameraExt, NULL);
    if (_rpClumpCameraExtOffset < 0) {
        return FALSE;
    }

    _rpClumpLightExtOffset = RpLightRegisterPlugin(sizeof(rpClumpObjectExt), rwID_CLUMP, ClumpInitLightExt,
                                                   ClumpDeInitLightExt, NULL);
    if (_rpClumpLightExtOffset < 0) {
        return FALSE;
    }

    return TRUE;
} // 0x802D01F8

/* Renders the clump's atomics that are flagged for rendering; NULL if any of them failed. */
RpClump* RpClumpRender(RpClump* clump) {
    RpClump* result = clump;
    RwLLLink* cur = rwLinkListGetFirstLLLink(&clump->atomicList);
    RwLLLink* end = rwLinkListGetTerminator(&clump->atomicList);

    while (cur != end) {
        RpAtomic* atomic = rwLLLinkGetData(cur, RpAtomic, inClumpLink);

        if (RpAtomicGetFlags(atomic) & rpATOMICRENDER) {
            RwFrameGetLTM(RpAtomicGetFrame(atomic));
            if (!RpAtomicRender(atomic)) {
                result = NULL;
            }
        }

        cur = rwLLLinkGetNext(cur);
    }

    return result;
} // 0x802D0274

/* Calls callback for each atomic of the clump until it returns NULL. */
RpClump* RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack callback, void* pData) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&clump->atomicList);
    RwLLLink* end = rwLinkListGetTerminator(&clump->atomicList);

    while (cur != end) {
        RwLLLink* next = rwLLLinkGetNext(cur);

        if (!callback(rwLLLinkGetData(cur, RpAtomic, inClumpLink), pData)) {
            return clump;
        }

        cur = next;
    }

    return clump;
} // 0x802D0304

/* Calls callback for each camera of the clump until it returns NULL. */
RpClump* RpClumpForAllCameras(RpClump* clump, RwCameraCallBack callback, void* pData) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&clump->cameraList);
    RwLLLink* end = rwLinkListGetTerminator(&clump->cameraList);

    while (cur != end) {
        RwLLLink* next = rwLLLinkGetNext(cur);
        RwCamera* camera = (RwCamera*)((RwUInt8*)rwLLLinkGetData(cur, rpClumpObjectExt, inClumpLink) -
                                       _rpClumpCameraExtOffset);

        if (!callback(camera, pData)) {
            return clump;
        }

        cur = next;
    }

    return clump;
} // 0x802D0374

/* Calls callback for each light of the clump until it returns NULL. */
RpClump* RpClumpForAllLights(RpClump* clump, RpLightCallBack callback, void* pData) {
    RwLLLink* cur = rwLinkListGetFirstLLLink(&clump->lightList);
    RwLLLink* end = rwLinkListGetTerminator(&clump->lightList);

    while (cur != end) {
        RwLLLink* next = rwLLLinkGetNext(cur);
        RpLight* light = (RpLight*)((RwUInt8*)rwLLLinkGetData(cur, rpClumpObjectExt, inClumpLink) -
                                    _rpClumpLightExtOffset);

        if (!callback(light, pData)) {
            return clump;
        }

        cur = next;
    }

    return clump;
} // 0x802D03EC

/*
 * Sets an atomic's geometry, taking a reference to the new one and releasing the old; the atomic's
 * bounding sphere becomes the geometry's first morph target's.
 */
RpAtomic* RpAtomicSetGeometry(RpAtomic* atomic, RpGeometry* geometry, RwUInt32 flags) {
    RwFrame* frame;

    if (geometry != atomic->geometry) {
        if (geometry != NULL) {
            _rpGeometryAddRef(geometry);
        }

        if (atomic->geometry != NULL) {
            RpGeometryDestroy(atomic->geometry);
        }

        atomic->geometry = geometry;
        if (geometry != NULL) {
            atomic->boundingSphere = geometry->morphTarget[0].boundingSphere;
        }

        frame = RpAtomicGetFrame(atomic);
        if (frame && RpAtomicGetWorld(atomic) != NULL) {
            RwFrameUpdateObjects(frame);
        }
    }

    return atomic;
} // no copy in retail (dead-stripped or inlined)

/* Creates an atomic with no geometry and no frame, rendered by the default callback. */
RpAtomic* RpAtomicCreate(void) {
    RpAtomic* atomic;

    atomic = (RpAtomic*)RwFreeListAlloc(RWCLUMPGLOBAL(atomicFreeList), rwMEMHINTDUR_EVENT | rwID_ATOMIC);
    if (atomic == NULL) {
        return NULL;
    }

    rwObjectInitialize(atomic, rpATOMIC, 0);
    atomic->object.sync = AtomicSync;
    atomic->repEntry = NULL;
    rwObjectSetFlags(atomic, rpATOMICCOLLISIONTEST | rpATOMICRENDER);
    rwObjectSetPrivateFlags(atomic, rpATOMICPRIVATEWORLDBOUNDDIRTY);
    RpAtomicSetFrame(atomic, NULL);

    atomic->geometry = NULL;
    atomic->boundingSphere.radius = 0.0f;
    atomic->boundingSphere.center.x = 0.0f;
    atomic->boundingSphere.center.y = 0.0f;
    atomic->boundingSphere.center.z = 0.0f;
    atomic->worldBoundingSphere.radius = 0.0f;
    atomic->worldBoundingSphere.center.x = 0.0f;
    atomic->worldBoundingSphere.center.y = 0.0f;
    atomic->worldBoundingSphere.center.z = 0.0f;

    RpAtomicSetRenderCallBack(atomic, AtomicDefaultRenderCallBack);

    atomic->interpolator.startMorphTarget = 0;
    atomic->interpolator.endMorphTarget = 0;
    atomic->interpolator.time = 1.0f;
    atomic->interpolator.recipTime = 1.0f;
    atomic->interpolator.position = 0.0f;
    atomic->interpolator.flags = rpINTERPOLATORDIRTYINSTANCE | rpINTERPOLATORDIRTYSPHERE;

    rwLLLinkInitialize(&atomic->inClumpLink);
    atomic->clump = NULL;
    atomic->pipeline = NULL;
    atomic->pad = 0x101;
    rwLinkListInitialize(&atomic->llWorldSectorsInAtomic);

    _rwPluginRegistryInitObject(&atomicTKList, atomic);

    return atomic;
} // 0x802D0464

/* Destroys an atomic: its plugin data, its instanced data, its geometry reference and its frame link. */
RwBool RpAtomicDestroy(RpAtomic* atomic) {
    _rwPluginRegistryDeInitObject(&atomicTKList, atomic);

    if (atomic->repEntry != NULL) {
        RwResourcesFreeResEntry(atomic->repEntry);
    }

    RpAtomicSetGeometry(atomic, NULL, 0);
    rwObjectHasFrameReleaseFrame(atomic);
    RwFreeListFree(RWCLUMPGLOBAL(atomicFreeList), atomic);

    return TRUE;
} // 0x802D05A4

/* Creates a copy of an atomic, sharing its geometry, with no frame and in no clump. */
RpAtomic* RpAtomicClone(RpAtomic* atomic) {
    RpAtomic* newAtomic;

    newAtomic = RpAtomicCreate();
    if (newAtomic == NULL) {
        return NULL;
    }

    rwObjectCopy(newAtomic, atomic);
    rwObjectSetFlags(newAtomic, RpAtomicGetFlags(atomic));
    RpAtomicSetFrame(newAtomic, NULL);
    RpAtomicSetGeometry(newAtomic, atomic->geometry, 0);

    newAtomic->boundingSphere = atomic->boundingSphere;
    newAtomic->renderCallBack = atomic->renderCallBack;
    newAtomic->pipeline = atomic->pipeline;
    newAtomic->interpolator = atomic->interpolator;
    newAtomic->pad = 0x101;

    _rwPluginRegistryCopyObject(&atomicTKList, newAtomic, atomic);

    return newAtomic;
} // 0x802D065C

/*
 * Creates a copy of a clump: a copy of its frame hierarchy and of each of its atomics, on the
 * copies of their frames. Its lights and cameras are not copied.
 */
RpClump* RpClumpClone(RpClump* clump) {
    rpClumpCloneData cloneData;

    cloneData.clump = clump;
    cloneData.clumpClone = RpClumpCreate();
    if (cloneData.clumpClone == NULL) {
        return NULL;
    }

    RpClumpSetFrame(cloneData.clumpClone, _rwFrameCloneAndLinkClones(RpClumpGetFrame(cloneData.clump)));
    if (!RpClumpGetFrame(cloneData.clumpClone)) {
        RpClumpDestroy(cloneData.clumpClone);
        return NULL;
    }

    cloneData.success = TRUE;
    RpClumpForAllAtomics(cloneData.clump, ClumpAtomicCloneAndAttach, &cloneData);
    _rwFramePurgeClone(RpClumpGetFrame(cloneData.clump));

    if (!cloneData.success) {
        RpClumpDestroy(cloneData.clumpClone);
        return NULL;
    }

    RpClumpSetCallBack(cloneData.clumpClone, cloneData.clump->callback);
    _rwPluginRegistryCopyObject(&clumpTKList, cloneData.clumpClone, cloneData.clump);

    return cloneData.clumpClone;
} // 0x802D07EC

/* Creates an empty clump with no frame. */
RpClump* RpClumpCreate(void) {
    RpClump* clump;

    clump = (RpClump*)RwFreeListAlloc(RWCLUMPGLOBAL(clumpFreeList), rwMEMHINTDUR_EVENT | rwID_CLUMP);
    if (clump == NULL) {
        return clump;
    }

    rwObjectInitialize(clump, rpCLUMP, 0);
    /* Clears the frame rwObjectInitialize already cleared, as RpAtomicCreate does with its call. */
    RpClumpSetFrame(clump, NULL);
    rwLinkListInitialize(&clump->atomicList);
    rwLinkListInitialize(&clump->lightList);
    rwLinkListInitialize(&clump->cameraList);
    rwLLLinkInitialize(&clump->inWorldLink);
    RpClumpSetCallBack(clump, ClumpCallBack);

    _rwPluginRegistryInitObject(&clumpTKList, clump);

    return clump;
} // 0x802D0A9C

/* Destroys a clump with its atomics, lights and cameras, and its frame hierarchy. */
RwBool RpClumpDestroy(RpClump* clump) {
    RwFrame* frame;

    _rwPluginRegistryDeInitObject(&clumpTKList, clump);

    RpClumpForAllAtomics(clump, ClumpDestroyAtomic, NULL);
    RpClumpForAllLights(clump, ClumpDestroyLight, NULL);
    RpClumpForAllCameras(clump, ClumpDestroyCamera, NULL);

    frame = RpClumpGetFrame(clump);
    if (frame) {
        RwFrameDestroyHierarchy(frame);
    }

    RwFreeListFree(RWCLUMPGLOBAL(clumpFreeList), clump);

    return TRUE;
} // 0x802D0B5C

/* Adds an atomic to a clump. */
RpClump* RpClumpAddAtomic(RpClump* clump, RpAtomic* atomic) {
    rwLinkListAddLLLink(&clump->atomicList, &atomic->inClumpLink);
    atomic->clump = clump;

    return clump;
} // 0x802D0C70

/* Adds a light to a clump. */
RpClump* RpClumpAddLight(RpClump* clump, RpLight* light) {
    rpClumpObjectExt* ext = CLUMPLIGHTEXTFROMLIGHT(light);

    rwLinkListAddLLLink(&clump->lightList, &ext->inClumpLink);
    ext->clump = clump;

    return clump;
} // no copy in retail (dead-stripped or inlined)

/* Adds a camera to a clump. */
RpClump* RpClumpAddCamera(RpClump* clump, RwCamera* camera) {
    rpClumpObjectExt* ext = CLUMPCAMERAEXTFROMCAMERA(camera);

    rwLinkListAddLLLink(&clump->cameraList, &ext->inClumpLink);
    ext->clump = clump;

    return clump;
} // no copy in retail (dead-stripped or inlined)

/* Takes a light out of its clump. */
RpClump* RpClumpRemoveLight(RpClump* clump, RpLight* light) {
    rpClumpObjectExt* ext = CLUMPLIGHTEXTFROMLIGHT(light);

    rwLinkListRemoveLLLink(&ext->inClumpLink);
    rwLLLinkInitialize(&ext->inClumpLink);
    ext->clump = NULL;

    return clump;
} // no copy in retail (dead-stripped or inlined)

/* Takes a camera out of its clump. */
RpClump* RpClumpRemoveCamera(RpClump* clump, RwCamera* camera) {
    rpClumpObjectExt* ext = CLUMPCAMERAEXTFROMCAMERA(camera);

    rwLinkListRemoveLLLink(&ext->inClumpLink);
    rwLLLinkInitialize(&ext->inClumpLink);
    ext->clump = NULL;

    return clump;
} // no copy in retail (dead-stripped or inlined)

/* Reads an atomic on its own (with its geometry, without a frame) from a stream. */
RpAtomic* RpAtomicStreamRead(RwStream* stream) {
    rwFrameList frameList;
    rpGeometryList geomList;

    frameList.numFrames = 0;
    geomList.numGeoms = 0;

    return ClumpAtomicStreamRead(stream, &frameList, &geomList);
} // 0x802D0C98

/*
 * Reads a clump from a stream: its frame hierarchy, its geometry list, then its atomics, lights
 * and cameras (each on a frame of the hierarchy), and its plugin data.
 */
RpClump* RpClumpStreamRead(RwStream* stream) {
    RwUInt32 length;
    RwUInt32 version;
    RwUInt32 listVersion;
    RpClumpChunkInfo clumpData;
    RpClump* clump;
    RwBool status;
    RpAtomic* atomic;
    RwInt32 i;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &length, &version)) {
        RWERROR((E_RW_READ));
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        rwFrameList frameList;
        rpGeometryList geomList;

        status = (RwStreamRead(stream, &clumpData, sizeof(clumpData)) == sizeof(clumpData));
        if (!status) {
            RWERROR((E_RW_READ));
            return NULL;
        }
        RwMemNative32(&clumpData, sizeof(clumpData));

        clump = RpClumpCreate();
        if (clump == NULL) {
            return NULL;
        }

        if (!RwStreamFindChunk(stream, rwID_FRAMELIST, NULL, &listVersion)) {
            RpClumpDestroy(clump);
            RWERROR((E_RW_READ));
            return NULL;
        }

        status = (_rwFrameListStreamRead(stream, &frameList) != NULL);
        if (!status) {
            RpClumpDestroy(clump);
            RWERROR((E_RW_READ));
            return NULL;
        }

        RpClumpSetFrame(clump, frameList.frames[0]);

        if (!RwStreamFindChunk(stream, rwID_GEOMETRYLIST, NULL, &listVersion)) {
            _rwFrameListDeinitialize(&frameList);
            RpClumpDestroy(clump);
            RWERROR((E_RW_READ));
            return NULL;
        }

        status = (GeometryListStreamRead(stream, &geomList) != NULL);
        if (!status) {
            _rwFrameListDeinitialize(&frameList);
            RpClumpDestroy(clump);
            RWERROR((E_RW_READ));
            return NULL;
        }

        for (i = 0; i < clumpData.numAtomics; i++) {
            status = RwStreamFindChunk(stream, rwID_ATOMIC, NULL, &version);
            if (status) {
                atomic = ClumpAtomicStreamRead(stream, &frameList, &geomList);
                status = (atomic != NULL);
            }

            if (!status) {
                GeometryListDeinitialize(&geomList);
                _rwFrameListDeinitialize(&frameList);
                RpClumpDestroy(clump);
                RWERROR((E_RW_READ));
                return NULL;
            }

            RpClumpAddAtomic(clump, atomic);
        }

        for (i = 0; i < clumpData.numLights; i++) {
            RwInt32 frameIndex;
            RpLight* light;

            status = RwStreamFindChunk(stream, rwID_STRUCT, NULL, NULL) &&
                     RwStreamReadInt32(stream, &frameIndex, sizeof(frameIndex));
            if (status) {
                status = RwStreamFindChunk(stream, rwID_LIGHT, NULL, NULL);
            }
            if (status) {
                light = RpLightStreamRead(stream);
                status = (light != NULL);
            }

            if (!status) {
                GeometryListDeinitialize(&geomList);
                _rwFrameListDeinitialize(&frameList);
                RpClumpDestroy(clump);
                RWERROR((E_RW_READ));
                return NULL;
            }

            RpLightSetFrame(light, frameList.frames[frameIndex]);
            RpClumpAddLight(clump, light);
        }

        for (i = 0; i < clumpData.numCameras; i++) {
            RwInt32 frameIndex;
            RwCamera* camera;

            status = RwStreamFindChunk(stream, rwID_STRUCT, NULL, NULL) &&
                     RwStreamReadInt32(stream, &frameIndex, sizeof(frameIndex));
            if (status) {
                status = RwStreamFindChunk(stream, rwID_CAMERA, NULL, NULL);
            }
            if (status) {
                camera = RwCameraStreamRead(stream);
                status = (camera != NULL);
            }

            if (!status) {
                GeometryListDeinitialize(&geomList);
                _rwFrameListDeinitialize(&frameList);
                RpClumpDestroy(clump);
                RWERROR((E_RW_READ));
                return NULL;
            }

            RwCameraSetFrame(camera, frameList.frames[frameIndex]);
            RpClumpAddCamera(clump, camera);
        }

        GeometryListDeinitialize(&geomList);
        _rwFrameListDeinitialize(&frameList);

        status = (_rwPluginRegistryReadDataChunks(&clumpTKList, stream, clump) != NULL);
        if (!status) {
            RpClumpDestroy(clump);
            RWERROR((E_RW_READ));
            return NULL;
        }

        return clump;
    } else {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }
} // 0x802D0CCC

/* Registers a plugin with atomics. */
RwInt32 RpAtomicRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                               RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&atomicTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x802D1A58

/* Registers a plugin with clumps. */
RwInt32 RpClumpRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                              RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&clumpTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x802D1A88

/* Registers the stream functions of an atomic plugin. */
RwInt32 RpAtomicRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                     RwPluginDataChunkWriteCallBack writeCB,
                                     RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&atomicTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x802D1AB8

/* Registers the function an atomic plugin's stream calls whether or not its chunk is present. */
RwInt32 RpAtomicSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB) {
    return _rwPluginRegistryAddPlgnStrmlwysCB(&atomicTKList, pluginID, alwaysCB);
} // 0x802D1AE0

/* Registers the function called when an atomic plugin is named in an atomic's rights chunk. */
RwInt32 RpAtomicSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB) {
    return _rwPluginRegistryAddPlgnStrmRightsCB(&atomicTKList, pluginID, rightsCB);
} // 0x802D1AF8

/* Where an atomic plugin's data sits in an atomic, or -1 if the plugin is not registered. */
RwInt32 RpAtomicGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&atomicTKList, pluginID);
} // 0x802D1B10

/* Attaches an atomic to a frame; its world bounding sphere becomes stale. */
RpAtomic* RpAtomicSetFrame(RpAtomic* atomic, RwFrame* frame) {
    rwObjectHasFrameSetFrame(atomic, frame);
    rwObjectSetPrivateFlags(atomic, rwObjectGetPrivateFlags(atomic) | rpATOMICPRIVATEWORLDBOUNDDIRTY);

    return atomic;
} // 0x802D1B20
