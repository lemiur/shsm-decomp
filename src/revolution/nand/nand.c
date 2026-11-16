// Reference: zeldaret/tp libs/revolution/src/nand/nand.c
#include <revolution/fs.h>
#include <revolution/nand.h>
#include <revolution/nand/NANDCore.h>
#include <revolution/nand/nand.h>
#include <revolution/os/OSError.h>

static BOOL nandInspectPermission(u8 perm);
static void nandSplitPerm(u8 perm, u32* ownerAcc, u32* groupAcc, u32* othersAcc);
static void nandGetStatusCallback(ISFSError result, void* ctxt);

/*
 * nandCreate
 *
 * Creates a file at a library path. Public paths only, unless the caller is privileged.
 */
static ISFSError nandCreate(const char* path, const u8 perm, const u8 attr, NANDCommandBlock* block,
                            BOOL async_flag, BOOL privilege_flag) {
    char absPath[64] = "";
    u32 owner = 0, group = 0, others = 0;

    nandGenerateAbsPath(absPath, path);

    if (!privilege_flag && nandIsPrivatePath(absPath)) {
        return ISFS_ERROR_ACCESS;
    } else if (!nandInspectPermission(perm)) {
        return ISFS_ERROR_INVALID;
    } else {
        nandSplitPerm(perm, &owner, &group, &others);

        if (async_flag) {
            return ISFS_CreateFileAsync((const u8*)absPath, attr, owner, group, others, nandCallback, block);
        } else {
            return ISFS_CreateFile((const u8*)absPath, attr, owner, group, others);
        }
    }
}

/*
 * NANDPrivateCreate
 */
s32 NANDPrivateCreate(const char* path, u8 perm, u8 attr) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    return nandConvertErrorCode(nandCreate(path, perm, attr, NULL, FALSE, TRUE));
} // 0x80243C80

/*
 * NANDCreateAsync
 *
 * Name invented after NANDPrivateCreateAsync (not in the reference).
 */
s32 NANDCreateAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandCreate(path, perm, attr, block, TRUE, FALSE));
} // 0x80243D70

/*
 * NANDPrivateCreateAsync
 */
s32 NANDPrivateCreateAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandCreate(path, perm, attr, block, TRUE, TRUE));
} // 0x80243E90

/*
 * nandDelete
 */
static ISFSError nandDelete(const char* path, NANDCommandBlock* block, BOOL async_flag, BOOL privilege_flag) {
    char absPath[64] = "";
    nandGenerateAbsPath(absPath, path);

    if (!privilege_flag && nandIsPrivatePath(absPath)) {
        return ISFS_ERROR_ACCESS;
    } else {
        if (async_flag) {
            return ISFS_DeleteAsync((const u8*)absPath, nandCallback, block);
        } else {
            return ISFS_Delete((const u8*)absPath);
        }
    }
}

/*
 * NANDDelete
 */
s32 NANDDelete(const char* path) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    return nandConvertErrorCode(nandDelete(path, NULL, FALSE, FALSE));
} // 0x80243F90

/*
 * NANDPrivateDelete
 */
s32 NANDPrivateDelete(const char* path) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    return nandConvertErrorCode(nandDelete(path, NULL, FALSE, TRUE));
} // 0x80244040

/*
 * NANDDeleteAsync
 *
 * Name invented after NANDPrivateDeleteAsync (not in the reference).
 */
s32 NANDDeleteAsync(const char* path, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandDelete(path, block, TRUE, FALSE));
} // 0x802440E0

/*
 * NANDPrivateDeleteAsync
 */
s32 NANDPrivateDeleteAsync(const char* path, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandDelete(path, block, TRUE, TRUE));
} // 0x802441C0

/*
 * NANDRead
 */
s32 NANDRead(NANDFileInfo* info, void* buf, const u32 length) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    return nandConvertErrorCode(ISFS_Read(info->fileDescriptor, (u8*)buf, length));
} // 0x80244280

/*
 * NANDReadAsync
 */
s32 NANDReadAsync(NANDFileInfo* info, void* buf, const u32 length, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(ISFS_ReadAsync(info->fileDescriptor, (u8*)buf, length, nandCallback, block));
} // 0x802442F0

/*
 * NANDWrite
 */
s32 NANDWrite(NANDFileInfo* info, const void* buf, const u32 length) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    return nandConvertErrorCode(ISFS_Write(info->fileDescriptor, (const u8*)buf, length));
} // 0x80244370

/*
 * NANDWriteAsync
 */
s32 NANDWriteAsync(NANDFileInfo* info, const void* buf, const u32 length, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(ISFS_WriteAsync(info->fileDescriptor, (const u8*)buf, length, nandCallback, block));
} // 0x802443E0

/*
 * nandSeek
 */
static ISFSError nandSeek(const IOSFd fd, const s32 offset, const s32 whence, NANDCommandBlock* block,
                          const BOOL async_flag) {
    u32 w = 0xFFFFFFFF; // invalid whence, left for IOS to reject

    switch (whence) {
    case 0:
        w = 0;
        break;
    case 1:
        w = 1;
        break;
    case 2:
        w = 2;
        break;
    default:
        break;
    }

    if (async_flag) {
        return ISFS_SeekAsync(fd, offset, w, nandCallback, block);
    } else {
        return ISFS_Seek(fd, offset, w);
    }
}

/*
 * NANDSeekAsync
 */
s32 NANDSeekAsync(NANDFileInfo* info, const s32 offset, const s32 whence, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandSeek(info->fileDescriptor, offset, whence, block, TRUE));
} // 0x80244460

/*
 * nandCreateDir
 */
static ISFSError nandCreateDir(const char* path, const u8 perm, const u8 attr, NANDCommandBlock* block,
                               const BOOL async_flag, const BOOL privilege_flag) {
    char absPath[64] = "";

    nandGenerateAbsPath(absPath, path);
    if (!privilege_flag && nandIsPrivatePath(absPath)) {
        return ISFS_ERROR_ACCESS;
    } else if (!nandInspectPermission(perm)) {
        return ISFS_ERROR_INVALID;
    } else {
        u32 owner = 0;
        u32 group = 0;
        u32 others = 0;
        nandSplitPerm(perm, &owner, &group, &others);

        if (async_flag) {
            return ISFS_CreateDirAsync((const u8*)absPath, attr, owner, group, others, nandCallback, block);
        } else {
            return ISFS_CreateDir((const u8*)absPath, attr, owner, group, others);
        }
    }
}

/*
 * NANDCreateDirAsync
 *
 * Name invented after NANDPrivateCreateDirAsync (not in the reference).
 */
s32 NANDCreateDirAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandCreateDir(path, perm, attr, block, TRUE, FALSE));
} // 0x80244510

/*
 * NANDPrivateCreateDirAsync
 */
s32 NANDPrivateCreateDirAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandCreateDir(path, perm, attr, block, TRUE, TRUE));
} // 0x80244630

/*
 * nandComposePerm
 *
 * Packs the owner, group and others access (bit 0 read, bit 1 write) into the permission byte.
 */
void nandComposePerm(u8* perm, const u32 ownerAcc, const u32 groupAcc, const u32 othersAcc) {
    u32 p = 0;

    if (ownerAcc & 1) {
        p = p | 0x10;
    }

    if (ownerAcc & 2) {
        p = p | 0x20;
    }

    if (groupAcc & 1) {
        p = p | 4;
    }

    if (groupAcc & 2) {
        p = p | 8;
    }

    if (othersAcc & 1) {
        p = p | 1;
    }

    if (othersAcc & 2) {
        p = p | 2;
    }

    ASSERTMSG((p & ~0xFF) == 0, "NAND library internal error.\n");
    *perm = (u8)p;
} // 0x80244730

/*
 * nandSplitPerm
 *
 * Unpacks the permission byte into owner, group and others access.
 */
static void nandSplitPerm(u8 perm, u32* ownerAcc, u32* groupAcc, u32* othersAcc) {
    *ownerAcc = 0;
    *groupAcc = 0;
    *othersAcc = 0;

    if (perm & 0x10) {
        *ownerAcc = *ownerAcc | 1;
    }

    if (perm & 0x20) {
        *ownerAcc = *ownerAcc | 2;
    }

    if (perm & 4) {
        *groupAcc = *groupAcc | 1;
    }

    if (perm & 8) {
        *groupAcc = *groupAcc | 2;
    }

    if (perm & 1) {
        *othersAcc = *othersAcc | 1;
    }

    if (perm & 2) {
        *othersAcc = *othersAcc | 2;
    }
} // 0x80244790

/*
 * nandGetStatus
 */
static ISFSError nandGetStatus(const char* path, NANDStatus* stat, NANDCommandBlock* block, const BOOL async_flag,
                               const BOOL privilege_flag) {
    char absPath[64] = "";

    nandGenerateAbsPath(absPath, path);
    if (!privilege_flag && nandIsUnderPrivatePath(absPath)) {
        return ISFS_ERROR_ACCESS;
    } else {
        if (async_flag) {
            return ISFS_GetAttrAsync((const u8*)absPath, &(stat->ownerId), &(stat->groupId), &(block->attr),
                                     &(block->ownerAcc), &(block->groupAcc), &(block->othersAcc),
                                     nandGetStatusCallback, block);
        } else {
            u32 attr = 0, ownerAcc = 0, groupAcc = 0, othersAcc = 0;
            ISFSError result = ISFS_GetAttr((const u8*)absPath, &stat->ownerId, &stat->groupId, &attr, &ownerAcc,
                                            &groupAcc, &othersAcc);
            if (result == ISFS_ERROR_OK) {
                nandComposePerm(&stat->permission, ownerAcc, groupAcc, othersAcc);
                stat->attribute = (u8)attr;
            }

            return result;
        }
    }
} // 0x80244820

/*
 * nandGetStatusCallback
 */
static void nandGetStatusCallback(ISFSError result, void* ctxt) {
    NANDCommandBlock* b = (NANDCommandBlock*)ctxt;
    if (result == ISFS_ERROR_OK) {
        NANDStatus* stat = b->status;
        stat->attribute = (u8)(b->attr);
        nandComposePerm(&(stat->permission), b->ownerAcc, b->groupAcc, b->othersAcc);
    }
    b->callback(nandConvertErrorCode(result), b);
} // 0x80244980

/*
 * NANDPrivateGetStatus
 */
s32 NANDPrivateGetStatus(const char* path, NANDStatus* stat) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    return nandConvertErrorCode(nandGetStatus(path, stat, NULL, FALSE, TRUE));
} // 0x80244A00

/*
 * NANDPrivateGetStatusAsync
 */
s32 NANDPrivateGetStatusAsync(const char* path, NANDStatus* stat, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    block->status = stat;
    return nandConvertErrorCode(nandGetStatus(path, stat, block, TRUE, TRUE));
} // 0x80244A70

/*
 * nandInspectPermission
 *
 * A file the owner cannot read is refused.
 */
static BOOL nandInspectPermission(const u8 perm) {
    if (perm & 0x10) {
        return TRUE;
    } else {
        return FALSE;
    }
}
