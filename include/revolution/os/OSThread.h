#ifndef REVOLUTION_OS_OSTHREAD_H
#define REVOLUTION_OS_OSTHREAD_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSTime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef s32 OSPriority;

#define OS_PRIORITY_MIN 0  /* highest */
#define OS_PRIORITY_MAX 31 /* lowest */
#define OS_PRIORITY_IDLE OS_PRIORITY_MAX

/* Written at the end of a thread stack; OSCheckActiveThreads tests it for overflow. */
#define OS_THREAD_STACK_MAGIC 0xDEADBABE

#define OS_THREAD_SPECIFIC_MAX 2

typedef struct OSThread OSThread;
typedef struct OSMutex OSMutex;

typedef struct OSThreadQueue {
    OSThread* head;
    OSThread* tail;
} OSThreadQueue;

typedef struct OSThreadLink {
    OSThread* next;
    OSThread* prev;
} OSThreadLink;

typedef struct OSMutexLink {
    OSMutex* next;
    OSMutex* prev;
} OSMutexLink;

typedef struct OSMutexQueue {
    OSMutex* head;
    OSMutex* tail;
} OSMutexQueue;

struct OSThread {
    OSContext context;       // 0x000
    u16 state;               // 0x2C8
    u16 attr;                // 0x2CA
    s32 suspend;             // 0x2CC: suspend count
    OSPriority priority;     // 0x2D0: effective priority
    OSPriority base;         // 0x2D4: base priority
    void* val;               // 0x2D8: exit value
    OSThreadQueue* queue;    // 0x2DC: queue the thread waits on
    OSThreadLink link;       // 0x2E0
    OSThreadQueue queueJoin; // 0x2E8: threads joining this one
    OSMutex* mutex;          // 0x2F0: mutex the thread waits for
    OSMutexQueue queueMutex; // 0x2F4: mutexes the thread holds
    OSThreadLink linkActive; // 0x2FC: link in __OSActiveThreadQueue
    u8* stackBase;           // 0x304
    u32* stackEnd;           // 0x308
    s32 error;               // 0x30C
    void* specific[OS_THREAD_SPECIFIC_MAX]; // 0x310
};

typedef void* (*OSThreadStartFunction)(void* arg);
typedef void (*OSSwitchThreadCallback)(OSThread* from, OSThread* to);
typedef void (*OSIdleFunction)(void* param);

/* Every thread that has been created and not yet exited, in low memory. */
OSThreadQueue __OSActiveThreadQueue : 0x800000DC;

/* The running thread, or NULL while the scheduler idles, in low memory. */
OSThread* __OSCurrentThread : 0x800000E4;

/* OSThread.state */
#define OS_THREAD_STATE_READY    1
#define OS_THREAD_STATE_RUNNING  2
#define OS_THREAD_STATE_WAITING  4
#define OS_THREAD_STATE_MORIBUND 8

/* OSThread.attr */
#define OS_THREAD_ATTR_DETACH 0x0001 /* released on exit; nobody joins it */

OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback);
void __OSThreadInit(void);
void OSInitThreadQueue(OSThreadQueue* queue);
OSThread* OSGetCurrentThread(void);
BOOL OSIsThreadSuspended(OSThread* thread);
BOOL OSIsThreadTerminated(OSThread* thread);
s32 OSDisableScheduler(void);
s32 OSEnableScheduler(void);
OSPriority __OSGetEffectivePriority(OSThread* thread);
void __OSPromoteThread(OSThread* thread, OSPriority priority);
void __OSReschedule(void);
void OSYieldThread(void);
BOOL OSCreateThread(OSThread* thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize,
                    OSPriority priority, u16 attr);
void OSExitThread(void* val);
void OSCancelThread(OSThread* thread);
BOOL OSJoinThread(OSThread* thread, void** val);
void OSDetachThread(OSThread* thread);
s32 OSResumeThread(OSThread* thread);
s32 OSSuspendThread(OSThread* thread);
void OSSleepThread(OSThreadQueue* queue);
void OSWakeupThread(OSThreadQueue* queue);
BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority);
OSPriority OSGetThreadPriority(OSThread* thread);
OSThread* OSSetIdleFunction(OSIdleFunction idleFunction, void* param, void* stack, u32 stackSize);
OSThread* OSGetIdleFunction(void);
s32 OSCheckActiveThreads(void);
void OSClearStack(u8 val);
void OSSetThreadSpecific(s32 index, void* ptr);
void* OSGetThreadSpecific(s32 index);
void OSSleepTicks(OSTime ticks);

#ifdef __cplusplus
}
#endif

#endif
