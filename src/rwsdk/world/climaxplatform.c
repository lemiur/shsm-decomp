// Unit name and boundary: Gormiti link map (RPworld.a climaxplatform)
// Names, parameters and locals: Gormiti DWARF; RwClimaxRVLSetVertexProcCallback: Gormiti link map
/*
 * climaxplatform.c: the Wii half of Climax's world plugin additions. Rendering the world raises
 * the render flags for the pass and, when the atomics are queued, resets and then measures their
 * pass lists (climaxcommon.c). Also the render callbacks the game hooks into the GX pipelines, the
 * conversion of a RenderWare matrix to a GX matrix and the pseudo HDR scaling of the vertex
 * colours of an instanced buffer.
 */
#include <revolution/gx/GXGeometry.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/badevice.h>
#include <rwsdk/driver/wii/dltextur.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/world/climaxcommon.h>
#include <rwsdk/world/climaxplatform.h>

/* The render callbacks the game installed. The compiler lays the zero-initialised small data out in reverse order of definition. */
RwClimaxRVLVertexProcCallBack _Current_RwClimaxVertexProcCB;
RwClimaxRVLTevCallBack _Current_RwClimaxRvlPostTevCB;
RwClimaxRVLTevCallBack _Current_RwClimaxRvlPreTevCB;
RwClimaxRVLTransformCallBack _Current_RwClimaxRvlTransformCB;

/*
 * Renders the world's sectors in the camera's frustum. Rendering the atomics needs their pass lists reset first, and
 * afterwards records how many of the buffer's entries the frame used and how many atomics did not fit.
 */
RpWorld* RwClimaxWorldRender(RpWorld* world, RwUInt32 flags) {
    RwClimaxWorldRenderFlags = flags | rwCLIMAXRENDERFLAGPASS;
    if (RwClimaxWorldRenderFlags & rwCLIMAXRENDERFLAGATOMICS) {
        int index;

        RwClimaxAtomics_OverflowCurrent = 0;
        RwClimaxCurrentAtomicRef = RwClimaxAtomicsBuffer;
        for (index = 0; index < rwCLIMAXNUMPASSES; index++) {
            RwClimaxAtomicPasses[index] = NULL;
        }
    }
    RwCameraForAllSectorsInFrustum(RWSRCGLOBAL(curCamera), RwClimaxWorldSectorRender, world);
    if (RwClimaxWorldRenderFlags & rwCLIMAXRENDERFLAGATOMICS) {
        /* The byte distance divided by sizeof, so the division (and the counters) are unsigned. */
        int AtomicRefsUsed =
            ((char*)RwClimaxCurrentAtomicRef - (char*)RwClimaxAtomicsBuffer) / sizeof(StAtomicRef);

        RwClimaxCurrentAtomicRef = NULL;
        RwClimaxAtomics_UsedCurrent = AtomicRefsUsed;
        if (RwClimaxAtomics_UsedCurrent > RwClimaxAtomics_UsedMax) {
            RwClimaxAtomics_UsedMax = RwClimaxAtomics_UsedCurrent;
        }
        if (RwClimaxAtomics_OverflowCurrent > RwClimaxAtomics_OverflowMax) {
            RwClimaxAtomics_OverflowMax = RwClimaxAtomics_OverflowCurrent;
        }
    }
    RwClimaxWorldRenderFlags = 0;
    return world;
} // 0x802DB9C8

/* Sets a texture into a texture map of the GX. A NULL texture is ignored. */
void RwClimaxSetTexture(RwTexture* texture, RwInt32 index) {
    if (texture != NULL) {
        _rwDlTextureSet(texture, index);
    }
} // 0x802DBAB0

/* Installs the game's transform and TEV callbacks. */
void RwClimaxRVLSetRenderCallbacks(RwClimaxRVLTransformCallBack transformCB, RwClimaxRVLTevCallBack preTevCB,
                                   RwClimaxRVLTevCallBack postTevCB) {
    _Current_RwClimaxRvlTransformCB = transformCB;
    _Current_RwClimaxRvlPreTevCB = preTevCB;
    _Current_RwClimaxRvlPostTevCB = postTevCB;
} // 0x802DBAC0

/* Installs the game's vertex processing callback. */
void RwClimaxRVLSetVertexProcCallback(RwClimaxRVLVertexProcCallBack vertexProcCB) {
    _Current_RwClimaxVertexProcCB = vertexProcCB;
} // 0x802DBAD0

/* Converts a RenderWare matrix (axes in rows) to a GX matrix (axes in columns, three rows). */
void RwClimaxRVLConvertMatrix(const RwMatrix* rwMatrix, Mtx mtx) {
    mtx[0][0] = rwMatrix->right.x;
    mtx[0][1] = rwMatrix->up.x;
    mtx[0][2] = rwMatrix->at.x;
    mtx[0][3] = rwMatrix->pos.x;

    mtx[1][0] = rwMatrix->right.y;
    mtx[1][1] = rwMatrix->up.y;
    mtx[1][2] = rwMatrix->at.y;
    mtx[1][3] = rwMatrix->pos.y;

    mtx[2][0] = rwMatrix->right.z;
    mtx[2][1] = rwMatrix->up.z;
    mtx[2][2] = rwMatrix->at.z;
    mtx[2][3] = rwMatrix->pos.z;
} // 0x802DBAD8

/*
 * Halves the three colour bytes of each element of an instanced colour array (the pseudo HDR lighting keeps the
 * colours in half range), then flushes the array to memory and invalidates the GX vertex cache. The first parameter
 * is not used; retail's caller passes a byte of the vertex format there.
 */
void RwClimaxRVLHDRPrelightApply(RwUInt8 vtxFmtByte, RxGameCubeVertexAttr* Attr) {
    int ind;
    unsigned char* pos;

    pos = (unsigned char*)Attr->array;
    for (ind = 0; ind < Attr->Count; ind++) {
        pos[0] = pos[0] >> 1;
        pos[1] = pos[1] >> 1;
        pos[2] = pos[2] >> 1;
        pos += Attr->stride;
    }
    DCFlushRange(Attr->array, Attr->stride * Attr->Count);
    GXInvalidateVtxCache();
} // 0x802DBB3C

/* Doubles the three colour bytes of each element of an instanced colour array again; the reverse of the apply above. */
void RwClimaxRVLHDRPrelightRemove(RwUInt8 vtxFmtByte, RxGameCubeVertexAttr* Attr) {
    int ind;
    unsigned char* pos;

    pos = (unsigned char*)Attr->array;
    for (ind = 0; ind < Attr->Count; ind++) {
        pos[0] = pos[0] << 1;
        pos[1] = pos[1] << 1;
        pos[2] = pos[2] << 1;
        pos += Attr->stride;
    }
    DCFlushRange(Attr->array, Attr->stride * Attr->Count);
    GXInvalidateVtxCache();
} // 0x802DBBB4
