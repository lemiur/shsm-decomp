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

/*
 * Records a trace entry against a new draw-sync token: a printf-style text, the object it is about and
 * a 16-bit value. Name unknown; Gormiti has no counterpart. Retail places it between dltextur's last
 * function and _rwDlTokenQueryDone, so its unit is unconfirmed; it is declared here because it hands
 * out the draw-sync token.
 */
extern void fn_802B37B8(const char* format, const void* object, RwUInt16 value);

#ifdef __cplusplus
}
#endif

#endif
