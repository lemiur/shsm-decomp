// Name and type: Gormiti DWARF (RwCore.a dldevice, driver/wii/dldevice.c)
#ifndef RWSDK_DRIVER_WII_DLDEVICE_H
#define RWSDK_DRIVER_WII_DLDEVICE_H

#include <rwsdk/plcore/bamatrix.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The number of frames the device has begun to render (what the skin cache stamps its entries with). */
extern int _RwSysFrameID;

/* The inverse of the camera's local transformation matrix, set up when a camera begins to update (the lights go into view space with it). */
extern RwMatrix _RwDlInvCamLTM;

/* Sets up the object-to-view transform (and normal matrix if normals is nonzero) in GX. */
extern void _rwDlTransformSetup(RwMatrix* ltm, int normals);

/* Calls a GX display list (cxGXCallDisplayList in Gormiti). */
extern void fn_802AEDC8(void* list, unsigned int nbytes);

#ifdef __cplusplus
}
#endif

#endif
