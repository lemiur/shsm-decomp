#ifndef REVOLUTION_OS_OSEXEC_H
#define REVOLUTION_OS_OSEXEC_H

#include <revolution/types.h>
#include <revolution/esp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Restart parameters handed from one program to the next (left at __OSExecParams). */
typedef struct OSExecParams {
    BOOL valid;
    u32 restartCode;
    u32 bootDol;
    void* regionStart;
    void* regionEnd;
    int argsUseDefault;
    void* argsAddr;
} OSExecParams;

/* Pointer to the restart parameters in low memory (at 0x800030F0): a heap address once a program has been started by OSExec. */
OSExecParams* __OSExecParams : 0x800030F0;

/* Offset of the apploader inside a TGC image, or 0 for a plain disc (0x800030F4). */
s32 __OSAppLoaderOffset : 0x800030F4;

/* Boot information the loader leaves in low memory (names and types unconfirmed). */
u8 __OSLockedFlag : 0x80003187;             /* 0x80 while the title runs from a locked (channel) partition */
u32 __OSLaunchPartitionType : 0x80003194;   /* type of the partition the title was launched from */
u32 __OSLaunchPartitionOffset : 0x80003198; /* its offset, 0 when none (name invented) */
u8 __OSDiscLayers : 0x8000319C;             /* 0x81 for a dual-layer disc (name invented) */

/* Partition type of the disc game to launch next; set from low memory 0x3194 on a reboot. */
extern u32 __OSNextPartitionType;

/* TRUE while a reboot (__OSReboot) is in progress. */
extern BOOL __OSInReboot;

void __OSGetExecParams(OSExecParams* params);
void __OSLaunchNextFirmware(void);
void __OSBootDolSimple(u32 doloffset, u32 restartCode, void* regionStart, void* regionEnd, BOOL argsUseDefault, s32 argc, char** argv);
void __OSBootDol(u32 doloffset, u32 restartCode, const char** argv);
void __OSLaunchMenu(void);
void __OSRelaunchTitle(u32 resetCode);

#ifdef __cplusplus
}
#endif

#endif
