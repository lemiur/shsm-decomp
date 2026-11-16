// Reference: tgsm/pbg src/rwsdk/src/badevice.c
/*
 * badevice.c: the engine. RwEngineInit, RwEngineOpen and RwEngineStart bring the core up one step
 * each (memory, strings and the core's modules; the device and the engine's globals block; the
 * plugins' globals), RwEngineStop, RwEngineClose and RwEngineTerm take the steps back; the other
 * RwEngine* functions pass requests to the device.
 */
#include <string.h>
#include <rwsdk/bacamera.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baframe.h>
#include <rwsdk/baimage.h>
#include <rwsdk/baraster.h>
#include <rwsdk/batextur.h>
#include <rwsdk/driver/gcn/dldevice.h>
#include <rwsdk/pipe/p2/baim3d.h>
#include <rwsdk/pipe/p2/bapipe.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/baresour.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/plcore/rwchunkgroup.h>
#include <rwsdk/plcore/rwstring.h>

/* The engine's plugin registry: the core's modules and the plugins keep their globals in its block. */
static RwPluginRegistry engineTKList = {
    sizeof(RwGlobals),
    sizeof(RwGlobals),
    0,
    0,
    NULL,
    NULL,
};

/* The globals until RwEngineOpen allocates the engine's block, and again after RwEngineClose. */
static RwGlobals staticGlobals;

void* RwEngineInstance;

/* The number of open engine instances. */
static RwInt32 engineInstancesOpened;

/* Registers the core's own modules with the engine; FALSE when any registration failed. */
static RwBool CorePluginAttach(void) {
    RwInt32 status = 0;

    status |= RwEngineRegisterPlugin(8, rwID_ERRORMODULE, _rwErrorOpen, _rwErrorClose);
    status |= RwEngineRegisterPlugin(24, rwID_VECTORMODULE, _rwVectorOpen, _rwVectorClose);
    status |= RwEngineRegisterPlugin(0, rwID_COLORMODULE, _rwColorOpen, _rwColorClose);
    status |= RwEngineRegisterPlugin(24, rwID_MATRIXMODULE, _rwMatrixOpen, _rwMatrixClose);
    status |= RwEngineRegisterPlugin(4, rwID_FRAMEMODULE, _rwFrameOpen, _rwFrameClose);
    status |= RwEngineRegisterPlugin(4, rwID_STREAMMODULE, _rwStreamModuleOpen, _rwStreamModuleClose);
    status |= RwEngineRegisterPlugin(4, rwID_CAMERAMODULE, _rwCameraOpen, _rwCameraClose);
    status |= RwEngineRegisterPlugin(544, rwID_IMAGEMODULE, _rwImageOpen, _rwImageClose);
    status |= RwEngineRegisterPlugin(100, rwID_RASTERMODULE, _rwRasterOpen, _rwRasterClose);
    status |= RwEngineRegisterPlugin(52, rwID_TEXTUREMODULE, _rwTextureOpen, _rwTextureClose);
    status |= RwEngineRegisterPlugin(96, rwID_PIPEMODULE, _rwRenderPipelineOpen, _rwRenderPipelineClose);
    status |= RwEngineRegisterPlugin(4, rwID_CHUNKGROUPMODULE, _rwChunkGroupOpen, _rwChunkGroupClose);
    status |= _rwPipeAttach();
    status |= RwEngineRegisterPlugin(124, rwID_IMMEDIATEMODULE, _rwIm3DOpen, _rwIm3DClose);
    status |= RwEngineRegisterPlugin(44, rwID_RESOURCESMODULE, _rwResourcesOpen, _rwResourcesClose);
    if (status >= 0) {
        return TRUE;
    }

    return FALSE;
} // 0x802937AC

/* The free list allocation without free lists: one heap block per entry. */
static void* MallocWrapper(RwFreeList* freeList, RwUInt32 hint) {
    return RwMalloc(freeList->entrySize, hint);
} // 0x80293A1C

/* The free list release without free lists. */
static RwFreeList* FreeWrapper(RwFreeList* freeList, void* mem) {
    RwFree(mem);

    return freeList;
} // 0x80293A30

/*
 * Moves the globals into a block of the engine's full size (every plugin is registered by now)
 * and opens the device; on failure the globals move back to staticGlobals.
 */
static RwBool EngineOpen(RwDevice* device, RwEngineOpenParams* initParams) {
    RwEngineInstance = RwMalloc(engineTKList.sizeOfStruct, rwMEMHINTDUR_GLOBAL);
    if (RwEngineInstance != NULL) {
        void* globals = RwEngineInstance;

        memcpy(globals, &staticGlobals, sizeof(RwGlobals));
        _rwDeviceSystemRequest(device, rwDEVICESYSTEMREGISTER, &RWSRCGLOBAL(dOpenDevice),
                               &RWSRCGLOBAL(memoryFuncs), 0);

        if (_rwDeviceSystemRequest(device, rwDEVICESYSTEMOPEN, NULL, initParams, 0)) {
            _rwDeviceSystemRequest(device, rwDEVICESYSTEMSTANDARDS, RWSRCGLOBAL(stdFunc), NULL,
                                   rwSTANDARDNUMOFSTANDARD);
            engineInstancesOpened++;
            return TRUE;
        }

        RwEngineInstance = &staticGlobals;
        memcpy(&staticGlobals, globals, sizeof(RwGlobals));
        RwFree(globals);
        return FALSE;
    }

    RwEngineInstance = &staticGlobals;
    RWERROR((E_RW_NOMEM, engineTKList.sizeOfStruct));
    return FALSE;
} // 0x80293A70

/*
 * Passes a request to the device. Requests the device refuses get the core's defaults: one sub
 * system, the rendering one.
 */
RwBool _rwDeviceSystemRequest(RwDevice* device, RwInt32 requestID, void* pOut, void* pInOut,
                              RwInt32 nIn) {
    RwBool result = device->fpSystem(requestID, pOut, pInOut, nIn);

    if (!result) {
        switch (requestID) {
            case rwDEVICESYSTEMFINALIZESTART:
            case rwDEVICESYSTEMINITIATESTOP:
                result = TRUE;
                break;
            case rwDEVICESYSTEMGETNUMSUBSYSTEMS:
                *(RwInt32*)pOut = 1;
                result = TRUE;
                break;
            case rwDEVICESYSTEMGETSUBSYSTEMINFO:
                result = (nIn == 0);
                if (result) {
                    rwstrcpy(((RwSubSystemInfo*)pOut)->name, "Only rendering sub system");
                }
                break;
            case rwDEVICESYSTEMGETCURRENTSUBSYSTEM:
                *(RwInt32*)pOut = 0;
                result = TRUE;
                break;
            case rwDEVICESYSTEMSETSUBSYSTEM:
                result = (nIn == 0);
                break;
        }
    }

    if (!result) {
        RWERROR((24, requestID));
    }

    return result;
} // 0x80293BC4

RwUInt32 _rwGetNumEngineInstances(void) {
    return engineInstancesOpened;
} // 0x80293CF0

/* The library's version, 3.7.0.2. */
RwUInt32 RwEngineGetVersion(void) {
    return 0x37002;
} // 0x80293CF8

/* Registers a plugin's globals with the engine; returns their offset in the engine's block. */
RwInt32 RwEngineRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor initCB,
                               RwPluginObjectDestructor termCB) {
    return _rwPluginRegistryAddPlugin(&engineTKList, size, pluginID, initCB, termCB, NULL);
} // 0x80293D04

/* The offset of a plugin's globals in the engine's block. */
RwInt32 RwEngineGetPluginOffset(RwUInt32 pluginID) {
    return _rwPluginRegistryGetPluginOffset(&engineTKList, pluginID);
} // 0x80293D30

/* The number of sub systems (graphics adapters) the device offers. */
RwInt32 RwEngineGetNumSubSystems(void) {
    RwInt32 numSubSystems = 1;

    _rwDeviceSystemRequest(&RWSRCGLOBAL(dOpenDevice), rwDEVICESYSTEMGETNUMSUBSYSTEMS,
                           &numSubSystems, NULL, 0);
    return numSubSystems;
} // 0x80293D40

/* Fills in a sub system's description; NULL when there is no such sub system. */
RwSubSystemInfo* RwEngineGetSubSystemInfo(RwSubSystemInfo* subSystemInfo, RwInt32 subSystemIndex) {
    if (!_rwDeviceSystemRequest(&RWSRCGLOBAL(dOpenDevice), rwDEVICESYSTEMGETSUBSYSTEMINFO,
                                subSystemInfo, NULL, subSystemIndex)) {
        subSystemInfo = NULL;
    }

    return subSystemInfo;
} // 0x80293D84

/* Selects the sub system the engine opens on. */
RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex) {
    return _rwDeviceSystemRequest(&RWSRCGLOBAL(dOpenDevice), rwDEVICESYSTEMSETSUBSYSTEM, NULL,
                                  NULL, subSystemIndex) != FALSE;
} // 0x80293DD8

/* The number of video modes, or -1 when the device cannot say. */
RwInt32 RwEngineGetNumVideoModes(void) {
    RwInt32 numModes;

    if (_rwDeviceSystemRequest(&RWSRCGLOBAL(dOpenDevice), rwDEVICESYSTEMGETNUMMODES, &numModes,
                               NULL, 0)) {
        return numModes;
    }

    return -1;
} // 0x80293E1C

/* Fills in a video mode's description; NULL when there is no such mode. */
RwVideoMode* RwEngineGetVideoModeInfo(RwVideoMode* modeinfo, RwInt32 modeIndex) {
    if (!_rwDeviceSystemRequest(&RWSRCGLOBAL(dOpenDevice), rwDEVICESYSTEMGETMODEINFO, modeinfo,
                                NULL, modeIndex)) {
        modeinfo = NULL;
    }

    return modeinfo;
} // 0x80293E68

/* Selects the video mode the engine starts in. */
RwBool RwEngineSetVideoMode(RwInt32 modeIndex) {
    return _rwDeviceSystemRequest(&RWSRCGLOBAL(dOpenDevice), rwDEVICESYSTEMUSEMODE, NULL, NULL,
                                  modeIndex) != FALSE;
} // 0x80293EBC

/* Stops the engine: the plugins' globals are torn down, then the device stops. */
RwBool RwEngineStop(void) {
    RwBool result;
    RwDevice* device = &RWSRCGLOBAL(dOpenDevice);

    _rwDeviceSystemRequest(device, rwDEVICESYSTEMINITIATESTOP, NULL, NULL, 0);
    _rwPluginRegistryDeInitObject(&engineTKList, RwEngineInstance);
    result = _rwDeviceSystemRequest(device, rwDEVICESYSTEMSTOP, NULL, NULL, 0);
    if (result) {
        RWSRCGLOBAL(engineStatus) = rwENGINESTATUSOPENED;
    }

    return result;
} // 0x80293F00

/* Starts the engine: the device starts, then every plugin's globals are set up. */
RwBool RwEngineStart(void) {
    RwDevice* device = &RWSRCGLOBAL(dOpenDevice);

    if (_rwDeviceSystemRequest(device, rwDEVICESYSTEMSTART, NULL, NULL, 0)) {
        if (_rwPluginRegistryInitObject(&engineTKList, RwEngineInstance)) {
            RwImageSetGamma(RWSRCGLOBAL(dOpenDevice).gammaCorrection);
            _rwDeviceSystemRequest(device, rwDEVICESYSTEMFINALIZESTART, NULL, NULL, 0);
            RWSRCGLOBAL(engineStatus) = rwENGINESTATUSSTARTED;
            return TRUE;
        }

        _rwDeviceSystemRequest(device, rwDEVICESYSTEMSTOP, NULL, NULL, 0);
    }

    return FALSE;
} // 0x80293F80

/* Closes the device and moves the globals back to staticGlobals. */
RwBool RwEngineClose(void) {
    RwBool result = _rwDeviceSystemRequest(&RWSRCGLOBAL(dOpenDevice), rwDEVICESYSTEMCLOSE, NULL,
                                           NULL, 0);

    if (result) {
        void* globals = RwEngineInstance;

        RwEngineInstance = &staticGlobals;
        memcpy(&staticGlobals, globals, sizeof(RwGlobals));
        RwFree(globals);
        engineInstancesOpened--;
        RWSRCGLOBAL(engineStatus) = rwENGINESTATUSINITED;
    }

    return result;
} // 0x80294038

/* Opens the device on the display initParams names. */
RwBool RwEngineOpen(RwEngineOpenParams* initParams) {
    RwBool result;

    if (RwEngineInstance == NULL) {
        RwEngineInstance = &staticGlobals;
    }

    result = (RWSRCGLOBAL(engineStatus) == rwENGINESTATUSINITED);
    if (result) {
        result = (initParams != NULL);
        if (result) {
            RwDevice* device = _rwDeviceGetHandle();

            result = (device != NULL);
            if (result) {
                result = EngineOpen(device, initParams);
                if (result) {
                    RWSRCGLOBAL(engineStatus) = rwENGINESTATUSOPENED;
                }
            }
        } else {
            RWERROR((E_RW_NULLP));
        }
    } else {
        RWERROR((E_RW_BADENGINESTATE));
    }

    return result;
} // 0x802940D8

/* Shuts the core down; refused while an engine instance is open. */
RwBool RwEngineTerm(void) {
    RwBool result = (engineInstancesOpened == 0);

    if (result) {
        _rwPluginRegistryClose();
        _rwMemoryClose();
        RWSRCGLOBAL(engineStatus) = rwENGINESTATUSIDLE;
    }

    return result;
} // 0x802941CC

/*
 * Initialises the core: the free list functions, the string table, the memory functions, the
 * plugin registry, the core's modules and the device's plugins. A failed step undoes the earlier
 * ones.
 */
RwBool RwEngineInit(const RwMemoryFunctions* memFuncs, RwUInt32 initFlags, RwUInt32 resArenaSize) {
    RwBool result = FALSE;

    RwEngineInstance = &staticGlobals;

    if (initFlags & rwENGINEINITNOFREELISTS) {
        RWSRCGLOBAL(memoryAlloc) = MallocWrapper;
        RWSRCGLOBAL(memoryFree) = FreeWrapper;
        _rwFreeListEnable(FALSE);
    } else {
        RWSRCGLOBAL(memoryAlloc) = _rwFreeListAllocReal;
        RWSRCGLOBAL(memoryFree) = _rwFreeListFreeReal;
        _rwFreeListEnable(TRUE);
    }

    RWSRCGLOBAL(resArenaInitSize) = resArenaSize;

    if (RWSRCGLOBAL(engineStatus) == rwENGINESTATUSIDLE) {
        result = _rwStringOpen();
        if (result) {
            result = _rwMemoryOpen(memFuncs);
            if (result) {
                result = _rwPluginRegistryOpen();
                if (result) {
                    result = CorePluginAttach();
                    if (result) {
                        result = _rwDeviceRegisterPlugin();
                        if (result) {
                            RWSRCGLOBAL(engineStatus) = rwENGINESTATUSINITED;
                            return result;
                        }
                    }
                    _rwPluginRegistryClose();
                }
                _rwMemoryClose();
            }
            _rwStringClose();
        }
    }

    return result;
} // 0x80294218
