#ifndef REVOLUTION_OS_H
#define REVOLUTION_OS_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSPhysicalToCached(paddr) ((void*)((u32)(paddr) + 0x80000000))
#define OSCachedToPhysical(caddr) ((u32)(caddr) - 0x80000000)

#define OSRoundUp32B(x) (((u32)(x) + 32 - 1) & ~(32 - 1))
#define OSRoundDown32B(x) (((u32)(x)) & ~(32 - 1))

/* Hardware setup called from __init_hardware at boot. */
void __OSPSInit(void);
void __OSFPRInit(void);
void __OSCacheInit(void);

/* Bounds of the system call handler that __OSInitSystemCall copies to the vector (labels in its asm body). */
void __OSSystemCallVectorStart(void);
void __OSSystemCallVectorEnd(void);
void __OSInitSystemCall(void);

/* TRUE while the system menu (IPL) is the running program. */
extern BOOL __OSInIPL;

/* DSP and audio clock bring-up and shutdown. */
void __OSInitAudioSystem(void);
void __OSStopAudioSystem(void);

/* Bounds of the MEM2 area reserved for IPC with Starlet. */
void* __OSGetIPCBufferHi(void);
void* __OSGetIPCBufferLo(void);
void __OSInitIPCBuffer(void);

#ifdef __cplusplus
}
#endif

#include <revolution/os/OS.h>
#include <revolution/os/OSAlarm.h>
#include <revolution/os/OSAlloc.h>
#include <revolution/os/OSArena.h>
#include <revolution/os/OSCache.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSError.h>
#include <revolution/os/OSException.h>
#include <revolution/os/OSExec.h>
#include <revolution/os/OSFastCast.h>
#include <revolution/os/OSFatal.h>
#include <revolution/os/OSFont.h>
#include <revolution/os/OSInterrupt.h>
#include <revolution/os/OSLink.h>
#include <revolution/os/OSMemory.h>
#include <revolution/os/OSMessage.h>
#include <revolution/os/OSMutex.h>
#include <revolution/os/OSNandbootInfo.h>
#include <revolution/os/OSNet.h>
#include <revolution/os/OSPlayRecord.h>
#include <revolution/os/OSPlayTime.h>
#include <revolution/os/OSReboot.h>
#include <revolution/os/OSSemaphore.h>
#include <revolution/os/OSStateTM.h>
#include <revolution/os/OSReset.h>
#include <revolution/os/OSRtc.h>
#include <revolution/os/OSStateFlags.h>
#include <revolution/os/OSThread.h>
#include <revolution/os/OSTime.h>
#include <revolution/os/OSUtf.h>

#endif
