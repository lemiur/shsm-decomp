// Unit name and boundary: Gormiti link map (RwCore.a baim3d)
// Names, locals, asserts and error codes: Gormiti DWARF and Debug build (baim3d.c).
/*
 * baim3d.c: 3D immediate mode. An application hands RwIm3DTransform its vertices, then draws
 * from them with RwIm3DRenderPrimitive or RwIm3DRenderIndexedPrimitive and closes the batch with
 * RwIm3DEnd; the module also holds the transform pipeline and the render pipeline of each
 * primitive type (the application's own, else the platform's).
 */
#include <rwsdk/pipe/p2/baim3d.h>
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/pipe/p2/wii/im3dpipe.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <string.h>

/* The most vertices one RwIm3DTransform takes. Name invented. */
#define IM3D_MAXVERTS 0x10000

/* The globals the module keeps in the engine instance, and its registration. */
RwModuleInfo _rwIm3DModule;
rwImmediGlobals* _rwIm3DGlobals;

/*
 * Transforms and lights the vertices through the transform pipeline and keeps them as the pool
 * the render calls draw from. The vertices come back on success, NULL on failure.
 */
void* RwIm3DTransform(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags) {
    RxPipeline* result;

    RWASSERT(0 < _rwIm3DModule.numInstances);
    RWASSERT(NULL != pVerts);

    if (numVerts > IM3D_MAXVERTS) {
        RWERROR((E_RX_TOOMANYVERTS));
    } else {
        RWIMMEDIGLOBAL(curPool).numElements = (RwUInt16)numVerts;
        RWIMMEDIGLOBAL(curPool).elements = pVerts;
        RWIMMEDIGLOBAL(curPool).stride = sizeof(RwIm3DVertex);
        RWIMMEDIGLOBAL(curPool).stash.ltm = ltm;
        RWIMMEDIGLOBAL(curPool).stash.flags = flags | rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA;

        result = RxPipelineExecute(RWIMMEDIGLOBAL(im3DTransformPipeline),
                                   &RWIMMEDIGLOBAL(curPool), TRUE);
        if (result) {
            return pVerts;
        }
    }

    return NULL;
} // 0x8029A898

/* Ends the batch RwIm3DTransform began; FALSE when none was begun. */
RwBool RwIm3DEnd(void) {
    RwBool im3dactive;

    RWASSERT(0 < _rwIm3DModule.numInstances);

    im3dactive = (RWIMMEDIGLOBAL(curPool).elements != NULL);
    if (!im3dactive) {
        return FALSE;
    }

    memset(&RWIMMEDIGLOBAL(curPool), 0, sizeof(rwIm3DPool));
    return TRUE;
} // 0x8029A978

/*
 * Draws the indexed primitives of the transformed vertices with the render pipeline of the
 * primitive type. A list type drops the indices that do not fill a primitive. An unknown
 * primitive type is reported, and the pipeline is then run with none (the original does).
 */
RwBool RwIm3DRenderIndexedPrimitive(RwPrimitiveType primType, RwImVertexIndex* indices,
                                    RwInt32 numIndices) {
    RwBool im3dactive;

    im3dactive = (RWIMMEDIGLOBAL(curPool).elements != NULL);

    RWASSERT(NULL != indices);

    if (im3dactive) {
        _rwIm3DPoolStash* stash = &RWIMMEDIGLOBAL(curPool).stash;

        stash->pipeline = NULL;
        stash->primType = primType;
        stash->indices = indices;
        stash->numIndices = numIndices;

        switch (primType) {
        case rwPRIMTYPETRILIST:
            RWASSERT(numIndices >= 3);
            RWASSERT((numIndices % 3) == 0);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).triList;
            stash->numIndices = numIndices - (numIndices % 3);
            break;
        case rwPRIMTYPETRIFAN:
            RWASSERT(numIndices >= 3);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).triFan;
            break;
        case rwPRIMTYPETRISTRIP:
            RWASSERT(numIndices >= 3);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).triStrip;
            break;
        case rwPRIMTYPELINELIST:
            RWASSERT(numIndices >= 2);
            RWASSERT((numIndices % 2) == 0);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).lineList;
            stash->numIndices = numIndices - (numIndices % 2);
            break;
        case rwPRIMTYPEPOLYLINE:
            RWASSERT(numIndices >= 2);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).polyLine;
            break;
        case rwPRIMTYPESPRITELIST:
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).spriteList;
            stash->numIndices = numIndices - (numIndices % 2);
            break;
        default:
            RWERROR((E_RX_INVALIDPRIMTYPE, primType));
            break;
        }

        if (RxPipelineExecute(stash->pipeline, stash, FALSE)) {
            return TRUE;
        }
    } else {
        RWERROR((E_RX_IM3DNOTACTIVE));
    }

    return FALSE;
} // 0x8029A9C8

/*
 * Draws the transformed vertices in order with the render pipeline of the primitive type. An
 * unknown primitive type is reported, and the pipeline is then run with none (the original does).
 */
RwBool RwIm3DRenderPrimitive(RwPrimitiveType primType) {
    RwBool im3dactive;
    struct RxHeap* heap;

    im3dactive = (RWIMMEDIGLOBAL(curPool).elements != NULL);
    heap = RxHeapGetGlobalHeap();
    RWASSERT(NULL != heap);

    if (im3dactive) {
        _rwIm3DPoolStash* stash = &RWIMMEDIGLOBAL(curPool).stash;

        stash->pipeline = NULL;
        stash->primType = primType;
        stash->indices = NULL;
        stash->numIndices = RWIMMEDIGLOBAL(curPool).numElements;

        switch (primType) {
        case rwPRIMTYPETRILIST:
            RWASSERT(RWIMMEDIGLOBAL(curPool).numElements >= 3);
            RWASSERT((RWIMMEDIGLOBAL(curPool).numElements % 3) == 0);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).triList;
            break;
        case rwPRIMTYPETRIFAN:
            RWASSERT(RWIMMEDIGLOBAL(curPool).numElements >= 3);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).triFan;
            break;
        case rwPRIMTYPETRISTRIP:
            RWASSERT(RWIMMEDIGLOBAL(curPool).numElements >= 3);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).triStrip;
            break;
        case rwPRIMTYPELINELIST:
            RWASSERT(RWIMMEDIGLOBAL(curPool).numElements >= 2);
            RWASSERT((RWIMMEDIGLOBAL(curPool).numElements % 2) == 0);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).lineList;
            break;
        case rwPRIMTYPEPOLYLINE:
            RWASSERT(RWIMMEDIGLOBAL(curPool).numElements >= 2);
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).polyLine;
            break;
        case rwPRIMTYPESPRITELIST:
            stash->pipeline = RWIMMEDIGLOBAL(im3DRenderPipelines).spriteList;
            break;
        default:
            RWERROR((E_RX_INVALIDPRIMTYPE, primType));
            break;
        }

        if (RxPipelineExecute(stash->pipeline, stash, FALSE)) {
            return TRUE;
        }
    } else {
        RWERROR((E_RX_IM3DNOTACTIVE));
    }

    return FALSE;
} // 0x8029AB9C

/*
 * Sets the pipeline vertices are transformed with; NULL restores the platform's. Returns the
 * pipeline now in use.
 */
RxPipeline* RwIm3DSetTransformPipeline(RxPipeline* pipeline) {
    if (pipeline) {
        RWIMMEDIGLOBAL(im3DTransformPipeline) = pipeline;
    } else {
        if (RWIMMEDIGLOBAL(platformIm3DTransformPipeline)) {
            RWIMMEDIGLOBAL(im3DTransformPipeline) = RWIMMEDIGLOBAL(platformIm3DTransformPipeline);
        } else {
            RWIMMEDIGLOBAL(im3DTransformPipeline) = NULL;
        }
    }

    return RWIMMEDIGLOBAL(im3DTransformPipeline);
} // 0x8029AD54

/*
 * Sets the pipeline a primitive type is rendered with; NULL restores the platform's. Returns
 * the pipeline now in use, NULL for an unknown primitive type. Quirk kept: when it restores the
 * platform's pipeline, the original returns the platform's own copy for every type from
 * tri strips on.
 */
RxPipeline* RwIm3DSetRenderPipeline(RxPipeline* pipeline, RwPrimitiveType primType) {
    if (pipeline) {
        switch (primType) {
        case rwPRIMTYPETRILIST:
            RWIMMEDIGLOBAL(im3DRenderPipelines).triList = pipeline;
            return pipeline;
        case rwPRIMTYPETRIFAN:
            RWIMMEDIGLOBAL(im3DRenderPipelines).triFan = pipeline;
            return pipeline;
        case rwPRIMTYPETRISTRIP:
            RWIMMEDIGLOBAL(im3DRenderPipelines).triStrip = pipeline;
            return pipeline;
        case rwPRIMTYPELINELIST:
            RWIMMEDIGLOBAL(im3DRenderPipelines).lineList = pipeline;
            return pipeline;
        case rwPRIMTYPEPOLYLINE:
            RWIMMEDIGLOBAL(im3DRenderPipelines).polyLine = pipeline;
            return pipeline;
        case rwPRIMTYPEPOINTLIST:
            RWIMMEDIGLOBAL(im3DRenderPipelines).pointList = pipeline;
            return pipeline;
        case rwPRIMTYPESPRITELIST:
            RWIMMEDIGLOBAL(im3DRenderPipelines).spriteList = pipeline;
            return pipeline;
        default:
            RWERROR((E_RX_INVALIDPRIMTYPE));
            break;
        }
    } else {
        switch (primType) {
        case rwPRIMTYPETRILIST:
            if (RWIMMEDIGLOBAL(platformIm3DRenderPipelines).triList) {
                RWIMMEDIGLOBAL(im3DRenderPipelines).triList =
                    RWIMMEDIGLOBAL(platformIm3DRenderPipelines).triList;
            } else {
                RWIMMEDIGLOBAL(im3DRenderPipelines).triList = NULL;
            }
            return RWIMMEDIGLOBAL(im3DRenderPipelines).triList;
        case rwPRIMTYPETRIFAN:
            if (RWIMMEDIGLOBAL(platformIm3DRenderPipelines).triFan) {
                RWIMMEDIGLOBAL(im3DRenderPipelines).triFan =
                    RWIMMEDIGLOBAL(platformIm3DRenderPipelines).triFan;
            } else {
                RWIMMEDIGLOBAL(im3DRenderPipelines).triFan = NULL;
            }
            return RWIMMEDIGLOBAL(im3DRenderPipelines).triFan;
        case rwPRIMTYPETRISTRIP:
            if (RWIMMEDIGLOBAL(platformIm3DRenderPipelines).triStrip) {
                RWIMMEDIGLOBAL(im3DRenderPipelines).triStrip =
                    RWIMMEDIGLOBAL(platformIm3DRenderPipelines).triStrip;
            } else {
                RWIMMEDIGLOBAL(im3DRenderPipelines).triStrip = NULL;
            }
            return RWIMMEDIGLOBAL(platformIm3DRenderPipelines).triStrip;
        case rwPRIMTYPELINELIST:
            if (RWIMMEDIGLOBAL(platformIm3DRenderPipelines).lineList) {
                RWIMMEDIGLOBAL(im3DRenderPipelines).lineList =
                    RWIMMEDIGLOBAL(platformIm3DRenderPipelines).lineList;
            } else {
                RWIMMEDIGLOBAL(im3DRenderPipelines).lineList = NULL;
            }
            return RWIMMEDIGLOBAL(platformIm3DRenderPipelines).lineList;
        case rwPRIMTYPEPOLYLINE:
            if (RWIMMEDIGLOBAL(platformIm3DRenderPipelines).polyLine) {
                RWIMMEDIGLOBAL(im3DRenderPipelines).polyLine =
                    RWIMMEDIGLOBAL(platformIm3DRenderPipelines).polyLine;
            } else {
                RWIMMEDIGLOBAL(im3DRenderPipelines).polyLine = NULL;
            }
            return RWIMMEDIGLOBAL(platformIm3DRenderPipelines).polyLine;
        case rwPRIMTYPEPOINTLIST:
            if (RWIMMEDIGLOBAL(platformIm3DRenderPipelines).pointList) {
                RWIMMEDIGLOBAL(im3DRenderPipelines).pointList =
                    RWIMMEDIGLOBAL(platformIm3DRenderPipelines).pointList;
            } else {
                RWIMMEDIGLOBAL(im3DRenderPipelines).pointList = NULL;
            }
            return RWIMMEDIGLOBAL(platformIm3DRenderPipelines).pointList;
        case rwPRIMTYPESPRITELIST:
            if (RWIMMEDIGLOBAL(platformIm3DRenderPipelines).spriteList) {
                RWIMMEDIGLOBAL(im3DRenderPipelines).spriteList =
                    RWIMMEDIGLOBAL(platformIm3DRenderPipelines).spriteList;
            } else {
                RWIMMEDIGLOBAL(im3DRenderPipelines).spriteList = NULL;
            }
            return RWIMMEDIGLOBAL(platformIm3DRenderPipelines).spriteList;
        default:
            RWERROR((E_RX_INVALIDPRIMTYPE));
            break;
        }
    }

    return NULL;
} // 0x8029ADA4

/* Registered as the immediate-mode module's close callback. */
void* _rwIm3DClose(void* instance, RwInt32 offset, RwInt32 size) {
    _rwIm3DDestroyPlatformRenderPipelines(&RWIMMEDIGLOBAL(platformIm3DRenderPipelines));
    _rwIm3DDestroyPlatformTransformPipeline(&RWIMMEDIGLOBAL(platformIm3DTransformPipeline));

    _rwIm3DModule.numInstances--;

    return instance;
} // 0x8029B0B8

/*
 * Registered as the immediate-mode module's open callback: clears the globals and builds the
 * platform's pipelines; if either fails, they are torn down again and NULL comes back.
 */
void* _rwIm3DOpen(void* instance, RwInt32 offset, RwInt32 size) {
    RwBool success;

    _rwIm3DModule.globalsOffset = offset;
    _rwIm3DGlobals = RWPLUGINOFFSET(rwImmediGlobals, RwEngineInstance, offset);
    _rwIm3DModule.numInstances++;

    memset(_rwIm3DGlobals, 0, sizeof(rwImmediGlobals));

    success = _rwIm3DCreatePlatformTransformPipeline(
        &RWIMMEDIGLOBAL(platformIm3DTransformPipeline));
    if (success) {
        success = _rwIm3DCreatePlatformRenderPipelines(
            &RWIMMEDIGLOBAL(platformIm3DRenderPipelines));
    }

    if (success) {
        return instance;
    }

    _rwIm3DDestroyPlatformRenderPipelines(&RWIMMEDIGLOBAL(platformIm3DRenderPipelines));
    _rwIm3DDestroyPlatformTransformPipeline(&RWIMMEDIGLOBAL(platformIm3DTransformPipeline));

    _rwIm3DModule.numInstances--;

    return NULL;
} // 0x8029B11C
