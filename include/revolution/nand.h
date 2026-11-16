#ifndef REVOLUTION_NAND_H
#define REVOLUTION_NAND_H

/*
 * Minimal NAND library header: only what the OS units use (state flags, play record).
 * SDK names and prototypes; to be completed by whoever reconstructs the NAND library.
 */

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAND_RESULT_OK 0
#define NAND_RESULT_ACCESS (-1)
#define NAND_RESULT_EXISTS (-6)
#define NAND_RESULT_INVALID (-8)
#define NAND_RESULT_BUSY (-3)
#define NAND_RESULT_MAXFD (-10)
#define NAND_RESULT_NOEXISTS (-12)
#define NAND_RESULT_ALLOC_FAILED (-2)
#define NAND_RESULT_CORRUPT (-4)
#define NAND_RESULT_ECC_CRIT (-5)
#define NAND_RESULT_MAXBLOCKS (-9)
#define NAND_RESULT_MAXFILES (-11)
#define NAND_RESULT_NOTEMPTY (-13)
#define NAND_RESULT_OPENFD (-14)
#define NAND_RESULT_AUTHENTICATION (-15)
#define NAND_RESULT_MAXDEPTH (-16)
#define NAND_RESULT_UNKNOWN (-64)
#define NAND_RESULT_FATAL_ERROR (-128)

#define NAND_MAX_PATH 64

/* Permission byte with read and write for owner, group and others. */
#define NAND_PERM_RWALL 0x3F

/* Access modes of NANDOpen. */
#define NAND_ACCESS_READ 1
#define NAND_ACCESS_WRITE 2
#define NAND_ACCESS_RW 3

/* Seek origins of NANDSeek. */
#define NAND_SEEK_BEG 0
#define NAND_SEEK_CUR 1
#define NAND_SEEK_END 2

typedef struct NANDFileInfo {
    s32 fileDescriptor;
    s32 origFd;
    char origPath[NAND_MAX_PATH];
    char tmpPath[NAND_MAX_PATH];
    u8 accType;
    u8 stage;
    u8 mark;
} NANDFileInfo;

/* File status as NANDGetStatus reports it. */
typedef struct NANDStatus {
    u32 ownerId;
    u16 groupId;
    u8 attribute;
    u8 permission;
} NANDStatus;

typedef struct NANDCommandBlock NANDCommandBlock;
typedef void (*NANDCallback)(s32 result, NANDCommandBlock* block);

struct NANDCommandBlock {
    void* userData;
    NANDCallback callback;
    NANDFileInfo* fileInfo;
    void* bytes;
    void* inodes;
    NANDStatus* status;
    u32 ownerId;
    u16 groupId;
    u8 nextStage;
    u32 attr;
    u32 ownerAcc;
    u32 groupAcc;
    u32 othersAcc;
    u32 num;
    char absPath[NAND_MAX_PATH];
    u32* length;
    u32* pos;
    int state;
    void* copyBuf;
    u32 bufLength;
    u8* type;
    u32 uniqNo;
    u32 reqBlocks;
    u32 reqInodes;
    u32* answer;
    u32 homeBlocks;
    u32 homeInodes;
    u32 userBlocks;
    u32 userInodes;
    u32 workBlocks;
    u32 workInodes;
    const char** dir;
    BOOL simpleFlag;
};

s32 NANDInit(void);
s32 NANDOpen(const char* path, NANDFileInfo* info, u8 accType);
s32 NANDOpenAsync(const char* path, NANDFileInfo* info, u8 accType, NANDCallback cb, NANDCommandBlock* block);
s32 NANDClose(NANDFileInfo* info);
s32 NANDCloseAsync(NANDFileInfo* info, NANDCallback cb, NANDCommandBlock* block);
s32 NANDRead(NANDFileInfo* info, void* buf, u32 length);
s32 NANDReadAsync(NANDFileInfo* info, void* buf, u32 length, NANDCallback cb, NANDCommandBlock* block);
s32 NANDWrite(NANDFileInfo* info, const void* buf, u32 length);
s32 NANDWriteAsync(NANDFileInfo* info, const void* buf, u32 length, NANDCallback cb, NANDCommandBlock* block);
s32 NANDSeekAsync(NANDFileInfo* info, s32 offset, s32 whence, NANDCallback cb, NANDCommandBlock* block);
s32 NANDDelete(const char* path);
s32 NANDDeleteAsync(const char* path, NANDCallback cb, NANDCommandBlock* block);
s32 NANDCreateAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block);
s32 NANDCreateDirAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block);
s32 NANDPrivateDeleteAsync(const char* path, NANDCallback cb, NANDCommandBlock* block);
s32 NANDPrivateGetStatusAsync(const char* path, NANDStatus* status, NANDCallback cb, NANDCommandBlock* block);
s32 NANDPrivateGetStatus(const char* path, NANDStatus* status);
s32 NANDPrivateDelete(const char* path);
s32 NANDPrivateCreate(const char* path, u8 perm, u8 attr);
s32 NANDPrivateOpen(const char* path, NANDFileInfo* info, u8 accType);
s32 NANDPrivateOpenAsync(const char* path, NANDFileInfo* info, u8 accType, NANDCallback cb, NANDCommandBlock* block);
s32 NANDPrivateCreateAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block);
s32 NANDPrivateCreateDirAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block);
s32 NANDSimpleSafeOpen(const char* path, NANDFileInfo* info, u8 accType, void* buf, u32 length);
s32 NANDSimpleSafeClose(NANDFileInfo* info);
s32 NANDSimpleSafeOpenAsync(const char* path, NANDFileInfo* info, u8 accType, void* buf, u32 length, NANDCallback cb, NANDCommandBlock* block);
s32 NANDSimpleSafeCloseAsync(NANDFileInfo* info, NANDCallback cb, NANDCommandBlock* block);

/* File types NANDPrivateGetTypeAsync reports (nandGetType: a directory read succeeds only on a directory). */
#define NAND_FILE_TYPE_FILE 1
#define NAND_FILE_TYPE_DIR 2

/* Channel banner as NANDInitBanner lays it out (0xF0A0 bytes). */
#define NAND_BANNER_TITLE_MAX 32
#define NAND_BANNER_SIGNATURE 0x5749424E /* 'WIBN' */

typedef struct NANDBanner {
    u32 signature;
    u32 flag;
    u16 iconSpeed;
    u8 reserved[22];
    u16 comment[2][NAND_BANNER_TITLE_MAX];
    u8 bannerTexture[0x6000];
    u8 iconTexture[8][0x1200];
} NANDBanner;

s32 NANDPrivateGetTypeAsync(const char* path, u8* type, NANDCallback cb, NANDCommandBlock* block);
void NANDInitBanner(NANDBanner* bnr, u32 flag, const u16* title, const u16* comment);

/* NANDCheck: answer bits 1 home blocks, 2 home inodes, 4 user blocks, 8 user inodes exceeded. */
s32 NANDCheckAsync(u32 fsBlock, u32 inode, u32* answer, NANDCallback cb, NANDCommandBlock* block);
s32 NANDGetFreeSpace(u32* freeBlocks, u32* freeInodes);

/* NANDLogging: result is TRUE when the whole log write completed. */
typedef void (*NANDLoggingCallback)(BOOL result);

BOOL NANDLoggingAddMessageAsync(NANDLoggingCallback cb, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
