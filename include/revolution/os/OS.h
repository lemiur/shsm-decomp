#ifndef REVOLUTION_OS_OS_H
#define REVOLUTION_OS_OS_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSException.h>
#include <revolution/os/OSExec.h>
#include <revolution/os/OSTime.h>
#include <revolution/gx.h>
#include <revolution/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Low-memory cached and uncached mirrors of physical memory. */
#define OS_CACHED_REGION_PREFIX   0x8000
#define OS_UNCACHED_REGION_PREFIX 0xC000

#define OS_BASE_CACHED   (OS_CACHED_REGION_PREFIX << 16)
#define OS_BASE_UNCACHED (OS_UNCACHED_REGION_PREFIX << 16)

#define OSPhysicalToUncached(paddr) ((void*)((u32)(OS_BASE_UNCACHED + (u32)(paddr))))

/* MEM1 and MEM2 are told apart by bits 28-29 of the address. */
#define OSIsMEM1Region(addr) (((u32)(addr) & 0x30000000) == 0x00000000)
#define OSIsMEM2Region(addr) (((u32)(addr) & 0x30000000) == 0x10000000)

/* Where the debugger interface (the DB integrator's mailbox) lives in low memory. */
#define OS_DBINTERFACE_ADDR 0x40

/* The boot information block at physical address 0. */
typedef struct OSBootInfo {
    DVDDiskID DVDDiskID;
    u32 magic;
    u32 version;
    u32 memorySize;
    u32 consoleType;
    void* arenaLo;
    void* arenaHi;
    void* FSTLocation;
    u32 FSTMaxLength;
} OSBootInfo;

/* IOS revision, decoded from the words the IOS leaves at 0x3140 and 0x3144. */
typedef struct OSIOSRev {
    u8 reserved;
    u8 major;
    u8 minor;
    u8 micro;
    u8 month;
    u8 date;
    u16 year;
} OSIOSRev;

/* Words the boot code and the drive inquiry leave in low memory. */
s32 ExiProbeStartTime[2] : 0x800030C0;
u16 __OSGCPad3Button : 0x800030E4; /* buttons held on GameCube pad 3 at boot, left by the apploader (name invented) */
vu16 __OSDeviceCode : 0x800030E6; // volatile: written by the drive inquiry callback

/* Labels inside the asm bodies of OSExceptionVector, __OSDBIntegrator and __OSDBJump: the code between each start/end pair is copied to low memory. */
void __OSEVStart(void);
void __OSEVSetNumber(void);
void __OSEVEnd(void);
void __DBVECTOR(void);
void __OSDBINTSTART(void);
void __OSDBINTEND(void);
void __OSDBJUMPSTART(void);
void __OSDBJUMPEND(void);

u32 OSGetConsoleType(void);
void OSInit(void);
void OSRegisterVersion(const char* version);
const char* OSGetAppGamename(void);
u8 OSGetAppType(void);
void __OSGetIOSRev(OSIOSRev* rev);

void OSDefaultExceptionHandler(__OSException exception, OSContext* context);
u32 __OSGetDIConfig(void);

/* Restart parameters left by the previous program, read at the top of OSInit. */
extern OSExecParams __OSRebootParams;

/* Start time of the OS, captured at the top of OSInit. */
extern OSTime __OSStartTime;

/* Hardware-detected board: TRUE on an arcade (GCAM) board. */
extern BOOL __OSIsGcam;

/* TRUE when the title was started from a NAND channel (nandboot). */
extern BOOL __OSInNandBoot;

#ifdef __cplusplus
}
#endif

#endif
