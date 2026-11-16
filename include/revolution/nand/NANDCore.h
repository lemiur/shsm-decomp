#ifndef REVOLUTION_NAND_NANDCORE_H
#define REVOLUTION_NAND_NANDCORE_H

#include <revolution/nand.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library-internal declarations owned by revolution/nand/NANDCore.c. */

BOOL nandIsPrivatePath(const char* path);
BOOL nandIsUnderPrivatePath(const char* path);
BOOL nandIsInitialized(void);
s32 nandConvertErrorCode(s32 error);
void nandGenerateAbsPath(char* absPath, const char* path);
void nandRemoveTailToken(char* newpath, const char* oldpath);
void nandGetHeadToken(char* token, char* newpath, const char* oldpath);
void nandConvertPath(char* abspath, const char* wd, const char* relpath);
void nandGetRelativeName(char* name, const char* path);
void nandGetParentDirectory(char* parent, const char* path);
void nandCallback(s32 result, void* ctxt);

const char* nandGetHomeDir(void);

extern const char* __NANDVersion;

#ifdef __cplusplus
}
#endif

#endif
