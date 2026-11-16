#ifndef REVOLUTION_OS_OSSTATETM_H
#define REVOLUTION_OS_OSSTATETM_H

#include <revolution/types.h>
#include <revolution/vi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*OSResetCallback)(void);
typedef void (*OSPowerCallback)(void);

OSResetCallback OSSetResetCallback(OSResetCallback callback);
OSPowerCallback OSSetPowerCallback(OSPowerCallback callback);

int __OSInitSTM(void);
void __OSShutdownToSBY(void);
void __OSHotReset(void);
int __OSSetVIForceDimming(BOOL isEnabled, u32 yShift, u32 xShift);
s32 __OSUnRegisterStateEvent(void);


#ifdef __cplusplus
}
#endif

#endif
