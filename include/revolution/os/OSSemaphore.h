#ifndef REVOLUTION_OS_OSSEMAPHORE_H
#define REVOLUTION_OS_OSSEMAPHORE_H

#include <revolution/types.h>
#include <revolution/os/OSThread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSSemaphore {
    volatile s32 count;  // 0x0. volatile: SDK declaration (doldecomp/brawl OpenRVL OSSemaphore.h); OSSignalSemaphore and other threads change it while OSWaitSemaphore sleeps
    OSThreadQueue queue; // 0x4: threads waiting for a positive count
} OSSemaphore;

void OSInitSemaphore(OSSemaphore* sem, s32 count);
s32 OSWaitSemaphore(OSSemaphore* sem);
s32 OSSignalSemaphore(OSSemaphore* sem);
s32 OSGetSemaphoreCount(OSSemaphore* sem);

#ifdef __cplusplus
}
#endif

#endif
