// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt and splits.txt (function names, unit layout)
/*
 * basector.c: RenderWare world sectors, the leaves of a world's BSP tree. The sector module only
 * keeps the plugin registry of world sectors and renders one through its world's callback.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/world/basector.h>
#include <rwsdk/world/baworld.h>

/* The plugins registered with world sectors: no plugin data yet, so a sector is just an RpWorldSector. */
RwPluginRegistry sectorTKList = {
    sizeof(RpWorldSector),
    sizeof(RpWorldSector),
    0,
    0,
    NULL,
    NULL,
};

/* The sector module's registration (only its instances are counted). */
static RwModuleInfo sectorModule;

/* Opens the sector module. */
void* _rpSectorOpen(void* instance, RwInt32 offset, RwInt32 size) {
    sectorModule.numInstances++;

    return instance;
} // 0x802D80C0

/* Closes the sector module. */
void* _rpSectorClose(void* instance, RwInt32 offset, RwInt32 size) {
    sectorModule.numInstances--;

    return instance;
} // 0x802D80D4

/* Renders the world sector through the current world's sector render callback. */
RpWorldSector* RpWorldSectorRender(RpWorldSector* worldSector) {
    RpWorld* world = (RpWorld*)RWSRCGLOBAL(curWorld);

    return world->renderCallBack(worldSector);
} // 0x802D80E8

/* Registers a plugin's data with every world sector. */
RwInt32 RpWorldSectorRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                                    RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return _rwPluginRegistryAddPlugin(&sectorTKList, size, pluginID, constructCB, destructCB, copyCB);
} // 0x802D80FC

/* Registers the stream callbacks of a plugin's world sector data. */
RwInt32 RpWorldSectorRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                          RwPluginDataChunkWriteCallBack writeCB,
                                          RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&sectorTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x802D812C
