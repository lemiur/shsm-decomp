#ifndef REVOLUTION_OS_OSNANDBOOTINFO_H
#define REVOLUTION_OS_OSNANDBOOTINFO_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Record of the title to return to, kept in /shared2/sys/NANDBOOTINFO (0x1020 bytes at the end of a 0x2000-byte buffer). */
typedef struct OSNandbootInfo {
    u32 CheckSum;
    u32 ArgOffset;
    u8 reserved0[2];
    u8 LastAppType;
    u8 ReturnType;
    u32 ArgValue;
    u8 reserved1[8];
    u64 LastTitleId;
    u8 reserved2[0x1000]; // pad: the rest of the record is not exercised by the OS library
} OSNandbootInfo;

BOOL __OSCreateNandbootInfo(void);
BOOL __OSWriteNandbootInfo(OSNandbootInfo* info);

#ifdef __cplusplus
}
#endif

#endif
