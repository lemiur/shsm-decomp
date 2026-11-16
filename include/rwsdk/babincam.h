// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h (declarations)
#ifndef RWSDK_BABINCAM_H
#define RWSDK_BABINCAM_H

#include <rwsdk/bacamera.h>
#include <rwsdk/plcore/bastream.h>

/*
 * A camera's structure chunk: its view window and offset, clip planes, fog distance and
 * projection.
 */
typedef struct rwStreamCamera RwCameraChunkInfo;
typedef struct rwStreamCamera rwStreamCamera;

struct rwStreamCamera {
    RwV2d viewWindow;       /* 0x0 */
    RwV2d viewOffset;       /* 0x8 */
    RwReal nearPlane;       /* 0x10 */
    RwReal farPlane;        /* 0x14 */
    RwReal fogPlane;        /* 0x18 */
    RwUInt32 projection;    /* 0x1C */
};

#ifdef __cplusplus
extern "C" {
#endif

RwInt32 RwCameraRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB,
                                     RwPluginDataChunkWriteCallBack writeCB,
                                     RwPluginDataChunkGetSizeCallBack getSizeCB);
RwCamera* RwCameraStreamRead(RwStream* stream);

#ifdef __cplusplus
}
#endif

#endif
