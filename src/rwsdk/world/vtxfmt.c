// Unit name and boundary: Gormiti link map (RPworld.a vtxfmt.obj; its source there is pipe/p2/gcn/vtxfmt.c)
// Names, parameters and locals: Gormiti DWARF (Final and Debug builds)
/*
 * vtxfmt.c: the GameCube vertex format plugin. A geometry or world may carry its own
 * RpGameCubeVtxFmt, the GX component types its instanced vertex arrays are packed in; without one
 * the default format applies. The plugin is registered with the engine (to set the default up),
 * with geometries and with worlds.
 */
#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/gx/GXManage.h>
#include <revolution/gx/GXVert.h>
#include <rwsdk/badevice.h>
#include <rwsdk/driver/wii/dltoken.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamateri.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/world/climaxplatform.h>
#include <rwsdk/world/gcpipe.h>
#include <rwsdk/world/vtxfmt.h>

/* The format of a geometry or world without one of its own; set up by the first open of the engine plugin. */
static RpGameCubeVtxFmt _RpDlVtxFmtDefault;

/*
 * Small uninitialised data is laid out in reverse order of definition: the module comes first so that it
 * lands after the two offsets.
 */
static RwModuleInfo _RpVtxFmtModule;
RwInt32 _rpDlWorldVtxFmtOffset;
RwInt32 _rpDlGeomVtxFmtOffset;

/*
 * ==================== fn_802DEE68 ====================
 * Sets the GX vertex descriptor, attribute formats and arrays for drawing an object's instanced vertex
 * buffer in the format fmt (the default one when fmt is NULL): positions always; normals, or normals,
 * binormals and tangents, when the object has normals; the colours when it is prelit, halving or
 * restoring them for the pseudo HDR lighting as the buffer asks; then one texture coordinate set per
 * remaining attribute array. _rwDlVtxFmtSetup in Gormiti's vtxfmt.c (and the name its trace text gives);
 * retail is another revision (0x274 bytes against 0x258).
 */
void fn_802DEE68(RpGameCubeVtxFmt* fmt, RxGameCubePipeData* pipeData) {
    unsigned int count;
    RxGameCubeVertexBuffer* vbHeader;

    if (fmt == NULL) {
        fmt = &_RpDlVtxFmtDefault;
    }

    vbHeader = (RxGameCubeVertexBuffer*)(pipeData->resEntry + 1);
    fn_802B37B8("_rwDlVtxFmtSetup(0x%08X,0) AttrArrays=%d", fmt, vbHeader->numAttrArrays);

    count = 0;

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, vbHeader->attr[count].indexType);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, fmt->pos, fmt->posFrac);
    GXSetArray(GX_VA_POS, vbHeader->attr[count].array, vbHeader->attr[count].stride);
    count++;

    if (pipeData->flags & rpGEOMETRYNORMALS) {
        if (fmt->nbt) {
            GXSetVtxDesc(GX_VA_NBT, vbHeader->attr[count].indexType);
            GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NBT, GX_NRM_NBT, fmt->norm, 0);
            GXSetArray(GX_VA_NBT, vbHeader->attr[count].array, vbHeader->attr[count].stride);
            count = 2;
        } else {
            GXSetVtxDesc(GX_VA_NRM, vbHeader->attr[count].indexType);
            GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, fmt->norm, 0);
            GXSetArray(GX_VA_NRM, vbHeader->attr[count].array, vbHeader->attr[count].stride);
            count = 2;
        }
    }

    if (pipeData->flags & rpGEOMETRYPRELIT) {
        GXSetVtxDesc(GX_VA_CLR0, vbHeader->attr[count].indexType);
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, fmt->preLight > GX_RGBX8 ? GX_CLR_RGBA : GX_CLR_RGB, fmt->preLight,
                        0);

        if (vbHeader->flags & rxGCVBHDRPRELIGHT) {
            if ((vbHeader->flags & rxGCVBHDRPRELIGHTAPPLIED) == 0) {
                RwClimaxRVLHDRPrelightApply(fmt->preLight, &vbHeader->attr[count]);
                vbHeader->flags |= rxGCVBHDRPRELIGHTAPPLIED;
            }
        } else if (vbHeader->flags & rxGCVBHDRPRELIGHTAPPLIED) {
            RwClimaxRVLHDRPrelightRemove(fmt->preLight, &vbHeader->attr[count]);
            vbHeader->flags &= ~rxGCVBHDRPRELIGHTAPPLIED;
        }

        GXSetArray(GX_VA_CLR0, vbHeader->attr[count].array, vbHeader->attr[count].stride);
        count++;
    }

    if (pipeData->flags & (rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) {
        GXAttr texCoord = GX_VA_TEX0;

        while (count < vbHeader->numAttrArrays) {
            GXSetVtxDesc(texCoord, vbHeader->attr[count].indexType);
            GXSetVtxAttrFmt(GX_VTXFMT0, texCoord, GX_TEX_ST, fmt->texCoord[texCoord - GX_VA_TEX0],
                            fmt->texCoordFrac[texCoord - GX_VA_TEX0]);
            GXSetArray(texCoord, vbHeader->attr[count].array, vbHeader->attr[count].stride);
            count++;
            texCoord++;
        }
    }

    GXFlush();
    GXWGFifo.u32 = 0;
    GXWGFifo.u32 = 0;
    GXWGFifo.u32 = 0;
    GXWGFifo.u32 = 0;
} // 0x802DEE68

/*
 * ==================== fn_802DF0DC ====================
 * The default vertex format. _rpGameCubeVtxFmtGetDefault in Gormiti's vtxfmt.c (the same place and size).
 */
RpGameCubeVtxFmt* fn_802DF0DC(void) {
    return &_RpDlVtxFmtDefault;
} // 0x802DF0DC

/*
 * ==================== _rxDlVertexFmtConst ====================
 * The geometry and world plugins' constructor: the object has no format of its own yet. Name: Gormiti's
 * vtxfmt.c has its function of this name at the same place and size.
 */
static void* _rxDlVertexFmtConst(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    *RWPLUGINOFFSET(RpGameCubeVtxFmt*, object, offsetInObject) = NULL;

    return object;
} // 0x802DF0E8

/*
 * ==================== _rxDlVertexFmtDest ====================
 * The geometry and world plugins' destructor: drops the object's reference to its format, freeing the
 * format with the last one.
 */
static void* _rxDlVertexFmtDest(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    RpGameCubeVtxFmt** vtxFmt = RWPLUGINOFFSET(RpGameCubeVtxFmt*, object, offsetInObject);

    if (*vtxFmt != NULL) {
        if ((*vtxFmt)->refCnt == 1) {
            RwFree(*vtxFmt);
        } else {
            (*vtxFmt)->refCnt--;
        }
    }

    return object;
} // 0x802DF0F4

/*
 * ==================== _rpDlVtxFmtOpen ====================
 * The engine plugin's open: the first one sets the default format up.
 */
static void* _rpDlVtxFmtOpen(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    if (++_RpVtxFmtModule.numInstances == 1) {
        RpGameCubeVtxFmtInit(&_RpDlVtxFmtDefault);
    }

    return object;
} // 0x802DF158

/*
 * ==================== _rpDlVtxFmtClose ====================
 * The engine plugin's close.
 */
static void* _rpDlVtxFmtClose(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    _RpVtxFmtModule.numInstances--;

    return object;
} // 0x802DF1A8

/*
 * ==================== _rpDlVtxFmtPluginAttach ====================
 * Registers the plugin with the engine, with geometries and with worlds; FALSE when one of them fails.
 */
RwInt32 _rpDlVtxFmtPluginAttach(void) {
    RwInt32 rpDlEngineVtxFmtOffset;

    rpDlEngineVtxFmtOffset =
        RwEngineRegisterPlugin(0, rwID_VERTEXFMTPLUGIN, _rpDlVtxFmtOpen, _rpDlVtxFmtClose);
    if (rpDlEngineVtxFmtOffset < 0) {
        return FALSE;
    }

    _rpDlGeomVtxFmtOffset = RpGeometryRegisterPlugin(sizeof(RpGameCubeVtxFmt*), rwID_VERTEXFMTPLUGIN,
                                                     _rxDlVertexFmtConst, _rxDlVertexFmtDest, NULL);
    if (_rpDlGeomVtxFmtOffset < 0) {
        return FALSE;
    }

    _rpDlWorldVtxFmtOffset = RpWorldRegisterPlugin(sizeof(RpGameCubeVtxFmt*), rwID_VERTEXFMTPLUGIN,
                                                   _rxDlVertexFmtConst, _rxDlVertexFmtDest, NULL);

    return _rpDlWorldVtxFmtOffset >= 0;
} // 0x802DF1BC

/*
 * ==================== RpGameCubeVtxFmtSetNormal ====================
 * Sets the component type of the normals, and whether binormals and tangents come with them.
 */
void RpGameCubeVtxFmtSetNormal(RpGameCubeVtxFmt* fmt, GXCompType type, int nbt) {
    fmt->norm = type;
    fmt->nbt = nbt;
} // 0x802DF26C

/*
 * ==================== RpGameCubeVtxFmtInit ====================
 * Resets a format to the default: floating point positions, normals and texture coordinates, RGBA8
 * colours, no binormals or tangents, one reference.
 */
void RpGameCubeVtxFmtInit(RpGameCubeVtxFmt* fmt) {
    int i;

    fmt->pos = GX_F32;
    fmt->norm = GX_F32;
    for (i = 0; i < 8; i++) {
        fmt->texCoord[i] = GX_F32;
    }
    fmt->preLight = GX_RGBA8;
    fmt->format = 1;
    fmt->posFrac = 0;
    for (i = 0; i < 8; i++) {
        fmt->texCoordFrac[i] = 0;
    }
    fmt->nbt = 0;
    fmt->refCnt = 1;
} // 0x802DF278

/*
 * ==================== RpGameCubeVtxFmtCreate ====================
 * A new format with the default settings and one reference.
 */
RpGameCubeVtxFmt* RpGameCubeVtxFmtCreate(void) {
    RpGameCubeVtxFmt* fmt;

    fmt = (RpGameCubeVtxFmt*)RwMalloc(sizeof(RpGameCubeVtxFmt), rwMEMHINTDUR_EVENT | rwID_WORLDPIPEMODULE);
    RpGameCubeVtxFmtInit(fmt);

    return fmt;
} // 0x802DF2E8

/*
 * ==================== RpGameCubeVtxFmtDestroy ====================
 * Drops one reference to a format, freeing it with the last.
 */
void RpGameCubeVtxFmtDestroy(RpGameCubeVtxFmt* fmt) {
    if (fmt->refCnt == 1) {
        RwFree(fmt);
    } else {
        fmt->refCnt--;
    }
} // 0x802DF334

/*
 * ==================== RpGameCubeGeometrySetVtxFmt ====================
 * Gives a geometry the format fmt, which gains a reference; the geometry's previous format loses one.
 */
void RpGameCubeGeometrySetVtxFmt(RpGeometry* geometry, RpGameCubeVtxFmt* fmt) {
    RpGameCubeVtxFmt** vtxFmt = RWPLUGINOFFSET(RpGameCubeVtxFmt*, geometry, _rpDlGeomVtxFmtOffset);

    if (*vtxFmt != NULL) {
        RpGameCubeVtxFmtDestroy(*vtxFmt);
    }

    *vtxFmt = fmt;
    fmt->refCnt++;
} // 0x802DF35C
