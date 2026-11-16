#ifndef REVOLUTION_OS_OSNET_H
#define REVOLUTION_OS_OSNET_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WiiConnect24 error codes: zero is success, negative values are errors. */
typedef s32 NWC24Err;

#define NWC24_OK 0

void __OSInitNet(void);

NWC24Err NWC24iPrepareShutdown(void);
NWC24Err NWC24iSynchronizeRtcCounter(BOOL notify);
NWC24Err NWC24SuspendScheduler(void);
NWC24Err NWC24iRequestShutdown(u32 event, s32* result);
BOOL NWC24Shutdown_(BOOL final, u32 event);
NWC24Err NWC24iSetRtcCounter_(s32 rtc, BOOL notify);

#ifdef __cplusplus
}
#endif

#endif
