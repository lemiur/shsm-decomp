#ifndef REVOLUTION_NAND_NANDOPENCLOSE_H
#define REVOLUTION_NAND_NANDOPENCLOSE_H

#include <revolution/fs.h>
#include <revolution/nand.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library-internal declarations owned by revolution/nand/NANDOpenClose.c. */

s32 nandSafeOpen(const char* path, NANDFileInfo* info, u8 accType, void* buf, u32 length, BOOL privilege_flag, BOOL simple_flag);
s32 nandSafeClose(NANDFileInfo* info, BOOL simple_flag);
s32 nandSafeOpenAsync(const char* path, NANDFileInfo* info, u8 accType, void* buf, u32 length, NANDCallback cb, NANDCommandBlock* block, BOOL privilege_flag, BOOL simple_flag);
s32 nandSafeCloseAsync(NANDFileInfo* info, NANDCallback cb, NANDCommandBlock* block, BOOL simple_flag);
void nandReadOpenCallback(ISFSError result, void* ctxt);

#ifdef __cplusplus
}
#endif

#endif
