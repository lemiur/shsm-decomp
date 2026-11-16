// Reference: tgsm/pbg config/GPLE9G/splits.txt (babincam.c, after babbox.c)
/*
 * babincam.c: cameras in streams: a camera is streamed as a structure chunk holding its view
 * window and offset, clip planes, fog distance and projection, then its plugin data.
 * RwCameraSetStreamAlwaysCallBack, RwCameraStreamGetSize, RwCameraStreamWrite and
 * RwCameraChunkInfoRead have no copy in retail (dead-stripped).
 */
#include <rwsdk/babincam.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/batkreg.h>
#include <string.h>

/* Registers a plugin's stream callbacks with cameras. */
RwInt32 RwCameraRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                     RwPluginDataChunkWriteCallBack writeCB,
                                     RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return _rwPluginRegistryAddPluginStream(&cameraTKList, pluginID, readCB, writeCB, getSizeCB);
} // 0x80290FB8

/*
 * Reads a camera whose chunk header the caller has already found: its structure chunk, then
 * its plugin data. A camera whose plugin data fails to read is not destroyed (the original
 * leaks it).
 */
RwCamera* RwCameraStreamRead(RwStream* stream) {
    RwUInt32 size;
    RwUInt32 version;
    rwStreamCamera cam;
    RwCamera* camera;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        memset(&cam, 0, sizeof(cam));
        if (RwStreamRead(stream, &cam, size) != size) {
            return NULL;
        }
        RwMemNative32(&cam, sizeof(cam));

        camera = RwCameraCreate();
        if (!camera) {
            return NULL;
        }

        if (!_rwPluginRegistryReadDataChunks(&cameraTKList, stream, camera)) {
            return NULL;
        }

        RwCameraSetViewWindow(camera, &cam.viewWindow);
        RwCameraSetViewOffset(camera, &cam.viewOffset);
        RwCameraSetNearClipPlane(camera, cam.nearPlane);
        RwCameraSetFarClipPlane(camera, cam.farPlane);
        RwCameraSetFogDistance(camera, cam.fogPlane);
        RwCameraSetProjection(camera, (RwCameraProjection)cam.projection);

        return camera;
    } else {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }
} // 0x80290FE0
