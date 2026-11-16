// Unit name: Gormiti link map (RwCore.a dltoken, driver/wii/dltoken.c)
#ifndef RWSDK_DRIVER_WII_DLTOKEN_H
#define RWSDK_DRIVER_WII_DLTOKEN_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The draw-sync token most recently handed out and the one the GX pipe last reported . */
extern volatile RwUInt16 _RwDlTokenCurrent; // volatile: retail reloads it at every use in _rwDlTokenQueryDone
extern volatile RwUInt16 _RwDlTokenLastSeen; // volatile: retail reloads it after the store in _rwDlTokenQueryDone

/* Whether the GX pipe has drawn up to the token. */
RwBool _rwDlTokenQueryDone(RwUInt16 token);

#ifdef __cplusplus
}
#endif

#endif
