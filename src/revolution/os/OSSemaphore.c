#include <revolution/os.h>

/*
 * Reference: doldecomp/brawl src/revolution/OS/OSSemaphore.c (function bodies) and
 * include/OpenRVL/include/revolution/OS/OSSemaphore.h (the volatile count).
 */

/*
 * OSInitSemaphore
 *
 * Sets the count of sem, with no waiting threads.
 */
void OSInitSemaphore(OSSemaphore* sem, s32 count) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    OSInitThreadQueue(&sem->queue);
    sem->count = count;
    OSRestoreInterrupts(enabled);
} // 0x80219FE0

/*
 * OSWaitSemaphore
 *
 * Waits for a positive count and decrements it. Returns the count before
 * the decrement.
 */
s32 OSWaitSemaphore(OSSemaphore* sem) {
    BOOL enabled;
    s32 count;

    enabled = OSDisableInterrupts();
    while ((count = sem->count) <= 0) {
        OSSleepThread(&sem->queue);
    }
    sem->count--;
    OSRestoreInterrupts(enabled);
    return count;
} // 0x8021A040

/*
 * OSSignalSemaphore
 *
 * Increments the count and wakes the waiters. Returns the count before the
 * increment.
 */
s32 OSSignalSemaphore(OSSemaphore* sem) {
    BOOL enabled;
    s32 count;

    enabled = OSDisableInterrupts();
    count = sem->count++;
    OSWakeupThread(&sem->queue);
    OSRestoreInterrupts(enabled);
    return count;
} // 0x8021A0B0

/*
 * OSGetSemaphoreCount
 *
 * Returns the count of sem.
 */
s32 OSGetSemaphoreCount(OSSemaphore* sem) {
    return sem->count;
} // 0x8021A110
