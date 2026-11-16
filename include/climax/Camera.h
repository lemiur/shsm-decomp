#ifndef CLIMAX_CAMERA_H
#define CLIMAX_CAMERA_H

// unit name unconfirmed: header name invented. The layout is RenderWare 3.7's RwCamera (RW SDK
// header rwcore.h), up to the members used so far; it stands in for the SDK header until it is
// added.

#include <climax/Atomic.h>
#include <climax/Frame.h>

/* A camera (RwCamera), up to its far plane (layout confirmed by Gormiti's DWARF for RwCamera). */
struct unk_RwCamera {
    u8              type;           // 0x00, RwObject
    u8              subType;        // 0x01
    u8              flags;          // 0x02
    u8              privateFlags;   // 0x03
    unk_RwFrame*    parent;         // 0x04, the camera's frame
    void*           lFrame[2];      // 0x08, RwObjectHasFrame's RwLLLink
    void*           sync;           // 0x10, RwObjectHasFrame's sync callback
    s32             projectionType; // 0x14
    void*           beginUpdate;    // 0x18
    void*           endUpdate;      // 0x1C
    unk_RwMatrix    viewMatrix;     // 0x20
    void*           frameBuffer;    // 0x60, an RwRaster
    void*           zBuffer;        // 0x64, an RwRaster
    unk_RwV2d       viewWindow;     // 0x68
    unk_RwV2d       recipViewWindow; // 0x70
    unk_RwV2d       viewOffset;     // 0x78
    float           nearPlane;      // 0x80
    float           farPlane;       // 0x84
};

/* The camera's frame (RwCameraGetFrame; the SDK's release build expands it as a macro). */
inline unk_RwFrame* RwCameraGetFrame( unk_RwCamera* camera ) {
    return camera->parent;
}

/* The camera's view matrix (RwCameraGetViewMatrix; a macro in the SDK's release build). */
inline unk_RwMatrix* RwCameraGetViewMatrix( unk_RwCamera* camera ) {
    return &camera->viewMatrix;
}

// defined in rwsdk/src/bacamera.c (parked): where sphere lies against the camera's view frustum
// (an RwFrustumTestResult: 0 outside, 1 on the boundary, 2 inside; C linkage). Open item:
// rwsdk/bacamera.h declares it too, on RwCamera.
extern "C" s32 RwCameraFrustumTestSphere( const unk_RwCamera* camera, const unk_RwSphere* sphere );

#endif
