// Reference: tgsm/pbg include/rwsdk/bacamera.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BACAMERA_H
#define RWSDK_BACAMERA_H

#include <rwsdk/babbox.h>
#include <rwsdk/baraster.h>
#include <rwsdk/batypehf.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/plcore/bavector.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The object type of a camera (rwID_CAMERA). */
#define rwCAMERA 4

/* RwCameraClearMode: the buffers RwCameraClear clears. */
typedef enum RwCameraClearMode {
    rwCAMERACLEARIMAGE = 0x1,
    rwCAMERACLEARZ = 0x2,
    rwCAMERACLEARSTENCIL = 0x4,
    rwCAMERACLEARMODEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwCameraClearMode;

/* RwCameraProjection: how a camera projects the world onto its view window. */
typedef enum RwCameraProjection {
    rwNACAMERAPROJECTION = 0,
    rwPERSPECTIVE = 1,
    rwPARALLEL = 2,
    rwCAMERAPROJECTIONFORCEENUMSIZEINT = 0x7FFFFFFF
} RwCameraProjection;

/* RwFrustumTestResult: where a sphere lies against a camera's view frustum. */
typedef enum RwFrustumTestResult {
    rwSPHEREOUTSIDE = 0,
    rwSPHEREBOUNDARY = 1,
    rwSPHEREINSIDE = 2,
    rwFRUSTUMTESTRESULTFORCEENUMSIZEINT = 0x7FFFFFFF
} RwFrustumTestResult;

/*
 * RwFrustumPlane: a plane of the view frustum, and for each axis which side of a bounding box
 * lies closest to it (1 where the normal's component is positive, 0 where it is negative).
 */
typedef struct RwFrustumPlane {
    RwPlane plane;      /* 0x0 */
    RwUInt8 closestX;   /* 0x10 */
    RwUInt8 closestY;   /* 0x11 */
    RwUInt8 closestZ;   /* 0x12 */
    RwUInt8 pad;        /* 0x13 */ // pad: bfbbdecomp/bfbb rwcore.h RwFrustumPlane member name
} RwFrustumPlane;

/* The SDK declares the typedef with the matrix alignment. */
typedef struct RwCamera RWALIGN(RwCamera, rwMATRIXALIGNMENT);

typedef RwCamera* (*RwCameraBeginUpdateFunc)(RwCamera* camera);
typedef RwCamera* (*RwCameraEndUpdateFunc)(RwCamera* camera);

/* Called for each camera of an iteration (bfbbdecomp/bfbb rwcore.h); NULL stops it. */
typedef RwCamera* (*RwCameraCallBack)(RwCamera* camera, void* data);

/*
 * RwCamera: a camera attached to a frame: its projection, the raster it renders to and its
 * z-buffer, its view window, offset and clip planes, the view matrix built from its frame's LTM,
 * and the frustum (planes, corners and bounding box) rebuilt whenever the frame moves.
 */
struct RwCamera {
    RwObjectHasFrame object;                /* 0x0 */
    RwCameraProjection projectionType;      /* 0x14 */
    RwCameraBeginUpdateFunc beginUpdate;    /* 0x18 */
    RwCameraEndUpdateFunc endUpdate;        /* 0x1C */
    RwMatrix viewMatrix;                    /* 0x20 */
    RwRaster* frameBuffer;                  /* 0x60 */
    RwRaster* zBuffer;                      /* 0x64 */
    RwV2d viewWindow;                       /* 0x68 */
    RwV2d recipViewWindow;                  /* 0x70 */
    RwV2d viewOffset;                       /* 0x78 */
    RwReal nearPlane;                       /* 0x80 */
    RwReal farPlane;                        /* 0x84 */
    RwReal fogPlane;                        /* 0x88 */
    RwReal zScale;                          /* 0x8C */
    RwReal zShift;                          /* 0x90 */
    RwFrustumPlane frustumPlanes[6];        /* 0x94 */
    RwBBox frustumBoundBox;                 /* 0x10C */
    RwV3d frustumCorners[8];                /* 0x124 */
};

#define RwCameraGetFrame(_camera) ((RwFrame*)rwObjectGetParent((_camera)))
#define RwCameraSetFrame(_camera, _frame) (_rwObjectHasFrameSetFrame((_camera), (_frame)), (_camera))

/* The fog distance, a plain member in release builds. */
#define RwCameraSetFogDistanceMacro(_camera, _distance) (((_camera)->fogPlane = (_distance)), (_camera))
#define RwCameraGetFogDistanceMacro(_camera) ((_camera)->fogPlane)
#define RwCameraSetFogDistance(_camera, _distance) RwCameraSetFogDistanceMacro(_camera, _distance)
#define RwCameraGetFogDistance(_camera) RwCameraGetFogDistanceMacro(_camera)

/* The plugins registered with cameras. */
extern RwPluginRegistry cameraTKList;

void* _rwCameraClose(void* instance, RwInt32 offset, RwInt32 size);
void* _rwCameraOpen(void* instance, RwInt32 offset, RwInt32 size);
RwCamera* RwCameraEndUpdate(RwCamera* camera);
RwCamera* RwCameraBeginUpdate(RwCamera* camera);
RwCamera* RwCameraSetViewOffset(RwCamera* camera, const RwV2d* offset);
RwCamera* RwCameraSetNearClipPlane(RwCamera* camera, RwReal nearClip);
RwCamera* RwCameraSetFarClipPlane(RwCamera* camera, RwReal farClip);
RwFrustumTestResult RwCameraFrustumTestSphere(const RwCamera* camera, const RwSphere* sphere);
RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearMode);
RwCamera* RwCameraShowRaster(RwCamera* camera, void* pDev, RwUInt32 flags);
RwCamera* RwCameraSetProjection(RwCamera* camera, RwCameraProjection projection);
RwCamera* RwCameraSetViewWindow(RwCamera* camera, const RwV2d* viewWindow);
RwInt32 RwCameraRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB,
                               RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwBool RwCameraDestroy(RwCamera* camera);
RwCamera* RwCameraCreate(void);
RwCamera* RwCameraClone(RwCamera* camera);

#ifdef __cplusplus
}
#endif

#endif
