// Reference: tgsm/pbg src/rwsdk/src/baraster.c
/*
 * baraster.c: RenderWare rasters. A raster's pixels live in the device driver, so each function
 * here hands the work to one of the driver's standard functions; the raster module's free list
 * hands out the raster structures. RwRasterRender and RwRasterRenderFast have no copy in retail
 * (dead-stripped).
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baraster.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/batkreg.h>

/* The free list of rasters; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rwRasterFreeList;

/* The raster module's registration (offset of its globals in the engine instance). */
static RwModuleInfo rasterModule;

/* Rasters per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rwRasterFreeListBlockSize = 128;
static RwInt32 _rwRasterFreeListPreallocBlocks = 1;

/* The number of entries of the raster context stack. */
#define rwRASTERCONTEXTSTACKSIZE 10

/*
 * The raster module's globals: the stack of rasters being rendered to (its bottom entry is the
 * dummy raster) and the raster free list. RWRASTERGLOBAL is invented.
 */
typedef struct rwRasterGlobals {
    RwRaster* rasterStack[rwRASTERCONTEXTSTACKSIZE];  /* 0x0 */
    RwInt32 rasterSP;                                 /* 0x28: the top of rasterStack */
    RwRaster dummyRaster;                             /* 0x2C */
    RwFreeList* rasterFreeList;                       /* 0x60 */
} rwRasterGlobals;

#define RWRASTERGLOBAL(var) \
    (RWPLUGINOFFSET(rwRasterGlobals, RwEngineInstance, rasterModule.globalsOffset)->var)

/* The plugins registered with rasters: none yet, so a raster is just an RwRaster. */
RwPluginRegistry rasterTKList = {
    sizeof(RwRaster),
    sizeof(RwRaster),
    0,
    0,
    NULL,
    NULL,
};

/* Unlocks the raster's pixels. */
RwRaster* RwRasterUnlock(RwRaster* raster) {
    RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERUNLOCK])(NULL, raster, 0);
    return raster;
} // 0x80297038

/* Unlocks the raster's palette. */
RwRaster* RwRasterUnlockPalette(RwRaster* raster) {
    RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERUNLOCKPALETTE])(NULL, raster, 0);
    raster->privateFlags &= ~rwRASTERPALETTELOCKED;
    return raster;
} // 0x80297080

/* Destroys the raster: its plugin data, the driver's raster and the structure. */
RwBool RwRasterDestroy(RwRaster* raster) {
    _rwPluginRegistryDeInitObject(&rasterTKList, raster);
    RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERDESTROY])(NULL, raster, 0);
    RwFreeListFree(RWRASTERGLOBAL(rasterFreeList), raster);
    return TRUE;
} // 0x802970D4

/* Registers a plugin with rasters; returns the offset of its data in a raster. */
RwInt32 RwRasterRegisterPlugin(RwInt32 size, RwUInt32 pluginID,
                               RwPluginObjectConstructor constructCB,
                               RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&rasterTKList, size, pluginID, constructCB, destructCB,
                                      copyCB);
} // 0x8029714C

/* Locks the raster's palette; returns the palette, or NULL when it cannot be locked. */
RwUInt8* RwRasterLockPalette(RwRaster* raster, RwInt32 lockMode) {
    RwUInt8* palette;

    if (RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERLOCKPALETTE])(&palette, raster, lockMode)) {
        return palette;
    }
    return NULL;
} // 0x8029717C

/* The number of mip levels of the raster: 1 without mipmaps, -1 when the driver cannot tell. */
RwInt32 RwRasterGetNumLevels(RwRaster* raster) {
    RwInt32 numLevels;

    if (!(RwRasterGetFormat(raster) & rwRASTERFORMATMIPMAP)) {
        return 1;
    }

    if (RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERGETMIPLEVELS])(&numLevels, raster, 0)) {
        return numLevels;
    }
    return -1;
} // 0x802971CC

/* Shows the camera raster on the device dev, after purging the resource arena. */
RwRaster* RwRasterShowRaster(RwRaster* raster, void* dev, RwUInt32 flags) {
    RwStandardFunc showRaster = RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERSHOWRASTER]);

    _rwResourcesPurge();

    if (showRaster(raster, dev, flags)) {
        return raster;
    }
    return NULL;
} // 0x8029722C

/*
 * Creates a raster of width x height pixels at depth bits per pixel; flags is the raster type
 * and format. Returns NULL when the driver cannot create it.
 */
RwRaster* RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags) {
    RwStandardFunc rasterCreate;
    RwRaster* raster;

    raster = (RwRaster*)RwFreeListAlloc(RWRASTERGLOBAL(rasterFreeList),
                                        rwMEMHINTDUR_EVENT | rwID_RASTERMODULE);
    if (raster != NULL) {
        rasterCreate = RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERCREATE]);
        raster->privateFlags = 0;
        raster->cFlags = 0;
        raster->width = width;
        raster->height = height;
        raster->nOffsetX = 0;
        raster->nOffsetY = 0;
        raster->depth = depth;
        raster->parent = raster;
        raster->cpPixels = NULL;
        raster->palette = NULL;

        if (!rasterCreate(NULL, raster, flags)) {
            RwFreeListFree(RWRASTERGLOBAL(rasterFreeList), raster);
            return NULL;
        }

        _rwPluginRegistryInitObject(&rasterTKList, raster);

        return raster;
    }

    return NULL;
} // 0x802972AC

/* Locks the pixels of the raster's mip level level; returns them, or NULL when they cannot be locked. */
RwUInt8* RwRasterLock(RwRaster* raster, RwUInt8 level, RwInt32 lockMode) {
    RwUInt8* pixels;

    if (RWSRCGLOBAL(stdFunc[rwSTANDARDRASTERLOCK])(&pixels, raster,
                                                   lockMode + ((RwInt32)level << 8))) {
        return pixels;
    }
    return NULL;
} // 0x802973A4

/* Closes the raster module: destroys the raster free list. */
void* _rwRasterClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWRASTERGLOBAL(rasterFreeList) != NULL) {
        RwFreeListDestroy(RWRASTERGLOBAL(rasterFreeList));
        RWRASTERGLOBAL(rasterFreeList) = NULL;
    }

    rasterModule.numInstances--;

    return instance;
} // 0x802973F8

/*
 * Opens the raster module: sets up the dummy raster at the bottom of the raster context stack
 * and creates the raster free list.
 */
void* _rwRasterOpen(void* instance, RwInt32 offset, RwInt32 size) {
    rasterModule.globalsOffset = offset;

    memset(&RWRASTERGLOBAL(dummyRaster), 0, sizeof(RwRaster));
    RWRASTERGLOBAL(dummyRaster).width = 0;
    RWRASTERGLOBAL(dummyRaster).height = 0;
    RWRASTERGLOBAL(dummyRaster).depth = 0;
    RWRASTERGLOBAL(dummyRaster).cFlags = rwRASTERDONTALLOCATE;
    RWRASTERGLOBAL(dummyRaster).cpPixels = NULL;
    RWRASTERGLOBAL(dummyRaster).palette = NULL;
    RWRASTERGLOBAL(dummyRaster).cType = rwRASTERTYPENORMAL;
    RWRASTERGLOBAL(rasterSP) = 0;
    RWRASTERGLOBAL(rasterStack[0]) = &RWRASTERGLOBAL(dummyRaster);

    RWRASTERGLOBAL(rasterFreeList) = RwFreeListCreateAndPreallocateSpace(
        rasterTKList.sizeOfStruct, _rwRasterFreeListBlockSize, sizeof(RwUInt32),
        _rwRasterFreeListPreallocBlocks, &_rwRasterFreeList, rwMEMHINTDUR_GLOBAL | rwID_RASTERMODULE);
    if (RWRASTERGLOBAL(rasterFreeList) == NULL) {
        return NULL;
    }

    rasterModule.numInstances++;

    return instance;
} // 0x80297464
