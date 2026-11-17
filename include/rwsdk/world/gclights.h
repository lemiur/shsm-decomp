// Name: Gormiti link map (RPworld.a gclights)
// Layout: Gormiti DWARF (_rwDlLightExt)
#ifndef RWSDK_WORLD_GCLIGHTS_H
#define RWSDK_WORLD_GCLIGHTS_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/world/balight.h>
#include <rwsdk/world/gcpipe.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The id of the light plugin of the GameCube world pipeline (chunk vendor 0). Name invented. */
#define rwID_DLLIGHTPLUGIN 0x505

/* The share of the three colour channels that count towards a light's intensity. */
#define rwGCLIGHTINTENSITYSHARE 0.3333f

/* The lights the hardware has (the GX light ids GX_LIGHT0 to GX_LIGHT7). Name invented. */
#define rwDLMAXHWLIGHTS 8

/*
 * _rwDlLightExt: the plugin data of a light: whether it uses the attenuation below (flags nonzero) or
 * the defaults, then the angle attenuation (a0 to a2) and the distance attenuation (k0 to k2) that
 * GX takes. Size 0x1C.
 */
typedef struct _rwDlLightExt {
    RwUInt32 flags; /* 0x00 */
    RwReal a0;      /* 0x04 */
    RwReal a1;      /* 0x08 */
    RwReal a2;      /* 0x0C */
    RwReal k0;      /* 0x10 */
    RwReal k1;      /* 0x14 */
    RwReal k2;      /* 0x18 */
} _rwDlLightExt;

/* Where the light plugin's data sits in a light. */
extern RwInt32 _RwDlLightExtOffset;

/* Applies the lights of the current world that light what lightFlags names (the world's or the atomics') to a pipeline's data. */
extern void _rwGCLightsGlobalEnable(RpLightFlag lightFlags, RxGameCubePipeData* pipeData);

/* Loads one light into the next hardware light of a pipeline's data. */
extern void _rwGCLightsLocalEnable(const RpLight* light, RxGameCubePipeData* pipeData);

/* The queue of the lights near the object being drawn: cleared, filled by contribution, and then applied to the hardware. */
extern void _rwGCLightsListInit(void);
extern void _rwGCLightsLocalQueue(const RpLight* light, float contrib);
extern void _rwGCLightsListFlush(RxGameCubePipeData* pipeData);

#ifdef __cplusplus
}
#endif

#endif
