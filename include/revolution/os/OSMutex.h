#ifndef REVOLUTION_OS_OSMUTEX_H
#define REVOLUTION_OS_OSMUTEX_H

#include <revolution/types.h>
#include <revolution/os/OSThread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OSMutex {
    OSThreadQueue queue; // 0x00: threads waiting for the mutex
    OSThread* thread;    // 0x08: owner, or NULL
    s32 count;           // 0x0C: lock depth of the owner
    OSMutexLink link;    // 0x10: link in the owner queueMutex
};

void OSInitMutex(OSMutex* mutex);
void OSLockMutex(OSMutex* mutex);
void OSUnlockMutex(OSMutex* mutex);
void __OSUnlockAllMutex(OSThread* thread);
BOOL __OSCheckDeadLock(OSThread* thread);
BOOL __OSCheckMutexes(OSThread* thread);

#ifdef __cplusplus
}
#endif

#endif
