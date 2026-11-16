// Reference: tgsm/pbg src/rwsdk/src/badevice.c (declarations)
#ifndef RWSDK_DRIVER_GCN_DLDEVICE_H
#define RWSDK_DRIVER_GCN_DLDEVICE_H

#include <rwsdk/badevice.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The device driver: the plugins it registers when the engine initialises, and its RwDevice.
 * Defined in dldevice.c, unit not yet reconstructed.
 */
RwBool _rwDeviceRegisterPlugin(void);
RwDevice* _rwDeviceGetHandle(void);

#ifdef __cplusplus
}
#endif

#endif
