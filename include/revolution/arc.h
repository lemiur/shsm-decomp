// Reference: zeldaret/tp libs/revolution/include/revolution/arc.h
#ifndef REVOLUTION_ARC_H
#define REVOLUTION_ARC_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Header of a U8 archive: the FST and the file data are located by offsets from the archive start. */
#define ARC_MAGIC 0x55AA382D

typedef struct ARCHeader {
    /* 0x00 */ u32 magic;
    /* 0x04 */ u32 fstStart;
    /* 0x08 */ u32 fstSize;
    /* 0x0C */ u32 fileStart;
    /* 0x10 */ u32 reserve[4];
} ARCHeader;

typedef struct ARCHandle {
    /* 0x00 */ void* archiveStartAddr;
    /* 0x04 */ void* FSTStart;
    /* 0x08 */ void* fileStart;
    /* 0x0C */ u32 entryNum;
    /* 0x10 */ char* FSTStringStart;
    /* 0x14 */ u32 FSTLength;
    /* 0x18 */ u32 currDir;
} ARCHandle;

typedef struct ARCFileInfo {
    /* 0x00 */ ARCHandle* handle;
    /* 0x04 */ u32 startOffset;
    /* 0x08 */ u32 length;
} ARCFileInfo;

typedef struct ARCDir {
    /* 0x00 */ ARCHandle* handle;
    /* 0x04 */ u32 entryNum;
    /* 0x08 */ u32 location;
    /* 0x0C */ u32 next;
} ARCDir;

typedef struct ARCDirEntry {
    /* 0x00 */ ARCHandle* handle;
    /* 0x04 */ u32 entryNum;
    /* 0x08 */ BOOL isDir;
    /* 0x0C */ char* name;
} ARCDirEntry;

BOOL ARCInitHandle(void* arcStart, ARCHandle* handle);
BOOL ARCOpen(ARCHandle* handle, const char* fileName, ARCFileInfo* af);
BOOL ARCFastOpen(ARCHandle* handle, s32 entrynum, ARCFileInfo* af);
s32 ARCConvertPathToEntrynum(ARCHandle* handle, const char* pathPtr);
void* ARCGetStartAddrInMem(ARCFileInfo* af);
u32 ARCGetLength(ARCFileInfo* af);
BOOL ARCClose(ARCFileInfo* af);
BOOL ARCChangeDir(ARCHandle* handle, const char* dirName);
BOOL ARCOpenDir(ARCHandle* handle, const char* dirName, ARCDir* dir);
BOOL ARCReadDir(ARCDir* dir, ARCDirEntry* dirent);
BOOL ARCCloseDir(ARCDir* dir);

#ifdef __cplusplus
}
#endif

#endif
