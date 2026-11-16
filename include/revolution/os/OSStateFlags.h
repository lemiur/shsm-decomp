#ifndef REVOLUTION_OS_OSSTATEFLAGS_H
#define REVOLUTION_OS_OSSTATEFLAGS_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Contents of /title/00000001/00000002/data/state.dat. */
typedef struct OSStateFlags {
    u32 checkSum;
    u8 lastBootApp;
    u8 lastShutdown;
    u8 lastDiscState;
    u8 menuMode;
    u8 reserved[24];
} OSStateFlags;

BOOL __OSWriteStateFlags(OSStateFlags* flags);
BOOL __OSReadStateFlags(OSStateFlags* flags);

#ifdef __cplusplus
}
#endif

#endif
