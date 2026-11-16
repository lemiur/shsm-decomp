// Reference: zeldaret/tp libs/revolution/src/fs/fs.c
//
// ISFS_CreateDir, ISFS_GetFileStats(Async) and ISFS_Seek are in the reference but have no retail address (nothing
// in the game calls them, so the linker dead-strips them); they are left out. ISFS_GetUsageAsync is not in the
// reference (name invented, after the ISFS_xxxAsync pairs): retail has it (the GetUsage callback case) and its shape
// follows ISFS_ReadDirAsync.
// The per-function callbacks (_FSGetStatsCb and friends) are inlined into _isfsFuncCb in retail.
#include <revolution/fs.h>
#include <revolution/ipc.h>
#include <revolution/os.h>

#include <string.h>

static IOSFd __fsFd = -1;
static u32 __fsInitialized = FALSE;
static char* __devfs = 0;
static IOSHeapId hId;
static s32 _asynCnt = 0;

#define ROUNDUP(sz) (((u32)(sz) + 31) & ~(u32)(31))

/* Returns a request buffer to the ISFS heap; the macro tests for NULL itself, and every caller tests as well. */
#define ISFS_FREE(ctxt)               \
    do {                          \
        if ((ctxt) != 0) {        \
            iosFree(hId, (ctxt)); \
        }                         \
    } while (0)

typedef struct isfs_GetAttr {
    IOSUid* ownerId;
    IOSGid* groupId;
    u32* attr;
    u32* ownerAcc;
    u32* groupAcc;
    u32* othersAcc;
} isfs_GetAttr;

typedef struct isfs_GetUsage {
    u32* nblocks;
    u32* ninodes;
} isfs_GetUsage;

typedef struct __isfsCtxt {
    u8 ioBuf[ROUNDUP(256)] ATTRIBUTE_ALIGN(32);
    ISFSCallback cb;
    void* ctxt;
    u32 func;

    union {
        ISFSStats* stats;
        ISFSFileStats* fstats;
        u32* num;
        isfs_GetAttr ga;
        isfs_GetUsage gu;
    } args;
} __isfsCtxt;

static IOSError _FSGetStatsCb(IOSError ret, void* ctxt);
static IOSError _FSReadDirCb(IOSError ret, void* ctxt);
static IOSError _FSGetAttrCb(IOSError ret, void* ctxt);
static IOSError _FSGetUsageCb(IOSError ret, void* ctxt);
static IOSError _FSGetFileStatsCb(IOSError ret, void* ctxt);

/*
 * ISFS_OpenLib
 *
 * Opens /dev/fs and carves the ISFS request heap (17 contexts) out of the IPC arena.
 */
ISFSError ISFS_OpenLib(void) {
    ISFSError rc = 0;
    static void* lo = 0, *hi = 0;
    __isfsCtxt* __fsCtxt = 0;

    if (!__fsInitialized) {
        lo = IPCGetBufferLo();
        hi = IPCGetBufferHi();
    }

    __devfs = (char*)ROUNDUP(lo);
    if (!__fsInitialized && ((u32)__devfs + ROUNDUP(64)) > (u32)hi) {
        OSReport("APP ERROR: Not enough IPC arena\n");
        rc = -22;
        goto out;
    }

    strcpy(__devfs, "/dev/fs");
    __fsFd = IOS_Open(__devfs, 0);

    if (__fsFd < 0) {
        rc = __fsFd;
        goto out;
    }

    __fsCtxt = (__isfsCtxt*)((u32)__devfs);

    if (!__fsInitialized && ((u32)__fsCtxt + (16 + 1) *  ROUNDUP(sizeof(__isfsCtxt))) > (u32)hi) {
        OSReport("APP ERROR: Not enough IPC arena\n");
        rc = -22;
        goto out;
    }

    if (!__fsInitialized) {
        IPCSetBufferLo((void*)((u32)__fsCtxt + (17) * ROUNDUP(sizeof(__isfsCtxt))));
        __fsInitialized = TRUE;
    }

    hId = iosCreateHeap(__fsCtxt, 17 * ROUNDUP(sizeof(__isfsCtxt)));

    if (hId < 0) {
        rc = -22;
        goto out;
    }

out:
    return rc;
} // 0x80240550

/*
 * _isfsFuncCb
 *
 * Completion callback of every async request: copies results out of the request buffer by request kind, calls the
 * caller callback, then frees the request context.
 */
IOSError _isfsFuncCb(IOSError ret, void* ctxt) {
    ISFSError rc = 0;
    __isfsCtxt* _ctxt = (__isfsCtxt*)ctxt;
    rc = ret;

    if (rc >= 0) {
        switch (_ctxt->func) {
        case 1:
            _FSGetStatsCb(ret, ctxt);
            break;
        case 2:
            _FSReadDirCb(ret, ctxt);
            break;
        case 3:
            _FSGetAttrCb(ret, ctxt);
            break;
        case 4:
            _FSGetUsageCb(ret, ctxt);
            break;
        case 5:
            _FSGetFileStatsCb(ret, ctxt);
            break;
        default:
            break;
        }
    }

    _asynCnt = 0;

    if (_ctxt->cb) {
        _ctxt->cb(rc, _ctxt->ctxt);
    }

    if (ctxt) {
        iosFree(hId, ctxt);
    }

    return rc;
} // 0x80240680

/*
 * ISFS_CreateDirAsync
 *
 * Async ioctl 3: create a directory.
 */
s32 ISFS_CreateDirAsync(const u8* dname, u32 dirAttr, u32 ownerAcc, u32 groupAcc, u32 othersAcc, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    u32 len;
    ISFSPathAttrArgs* pathAttrArgs;
    __isfsCtxt* ctxt;

    if (dname == NULL || __fsFd < 0 || (len = strnlen(dname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);

    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    pathAttrArgs = (ISFSPathAttrArgs*)ctxt->ioBuf;
    memcpy(pathAttrArgs->path, (void*)dname, len + 1);
    pathAttrArgs->attr = dirAttr;
    pathAttrArgs->ownerAccess = ownerAcc;
    pathAttrArgs->groupAccess = groupAcc;
    pathAttrArgs->othersAccess = othersAcc;
    rc = IOS_IoctlAsync(__fsFd, 3, pathAttrArgs, sizeof(*pathAttrArgs), NULL, 0, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80240800

/*
 * ISFS_ReadDir
 *
 * Ioctlv 4: list a directory (or just count its entries when nameList is NULL).
 */
s32 ISFS_ReadDir(const u8* dname, u8* nameList, u32* num) {
    ISFSError rc = ISFS_ERROR_OK;
    IOSIoVector* v = 0;
    u32 len, numInputs, numOutputs, *numPtr;
    char* dnPtr;
    __isfsCtxt* blCtxt = 0;

    if (dname == NULL || num == NULL || __fsFd < 0 || (u32)nameList & 31 || (len = strnlen(dname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    blCtxt = iosAllocAligned(hId, sizeof(*blCtxt), 32);
    if (blCtxt == 0) {
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    v = (IOSIoVector*)blCtxt->ioBuf;
    dnPtr = (char*)ROUNDUP((u8*)&v[4]);
    memcpy(dnPtr, dname, len + 1);
    v[0].base = (u8*)dnPtr;
    v[0].length = 64;
    numPtr = (u32*)ROUNDUP((u32)dnPtr + 64);
    v[1].base = (u8*)numPtr;
    v[1].length = 4;

    if (nameList != 0) {
        numInputs = 2;
        numOutputs = 2;
        *numPtr = *num;
        v[2].base = nameList;
        v[2].length = *num * 13;
        v[3].base = (u8*)numPtr;
        v[3].length = 4;
    } else {
        numInputs = 1;
        numOutputs = 1;
    }

    rc = IOS_Ioctlv(__fsFd, 4, numInputs, numOutputs, v);

    if (rc != ISFS_ERROR_OK) {
        goto out;
    }

    *num = *numPtr;

out:
    if (blCtxt != 0) {
        ISFS_FREE(blCtxt);
    }

    return rc;
} // 0x80240900

/*
 * ISFS_ReadDirAsync
 *
 * Async ReadDir.
 */
s32 ISFS_ReadDirAsync(const u8* dname, u8* nameList, u32* num, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    u32 len, numOutputs, numInputs, *numPtr;
    IOSIoVector* v;
    __isfsCtxt* ctxt;
    char* dnPtr;
    
    if (dname == NULL || num == NULL || __fsFd < 0 || (u32)nameList & 31 || (len = strnlen(dname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 2;
    ctxt->args.num = num;
    v = (IOSIoVector*)ctxt->ioBuf;
    dnPtr = (char*)ROUNDUP((u8*)&v[4]);
    memcpy(dnPtr, dname, len + 1);
    v[0].base = (u8*)dnPtr;
    v[0].length = 64;
    numPtr = (u32*)ROUNDUP((u32)dnPtr + 64);
    v[1].base = (u8*)numPtr;
    v[1].length = 4;

    if (nameList != 0) {
        numInputs = 2;
        numOutputs = 2;
        *numPtr = *num;
        v[2].base = nameList;
        v[2].length = *num * 13;
        v[3].base = (u8*)numPtr;
        v[3].length = 4;
    } else {
        numInputs = 1;
        numOutputs = 1;
    }

    rc = IOS_IoctlvAsync(__fsFd, 4, numInputs, numOutputs, v, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80240A60

/*
 * ISFS_GetAttr
 *
 * Ioctl 6: owner, group, attribute and access bits of a path.
 */
s32 ISFS_GetAttr(const u8* name, IOSUid* ownerId, IOSGid* groupId, u32* attr, u32* ownerAcc, u32* groupAcc, u32* othersAcc) {
    ISFSError rc = ISFS_ERROR_OK;
    ISFSPathAttrArgs* pathAttrArgs;
    u8* ptr;
    u32 len;
    __isfsCtxt* blCtxt = 0;

    if (name == NULL || __fsFd < 0 ||
            (len = strnlen(name, 64)) == 64 ||
            ownerId == NULL || groupId == NULL || attr == NULL ||
            ownerAcc == NULL || groupAcc == NULL || othersAcc == NULL) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    blCtxt = iosAllocAligned(hId, sizeof(*blCtxt), 32);
    if (blCtxt == 0) {
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    ptr = (u8*)blCtxt->ioBuf;
    memcpy(ptr, name, len + 1);
    pathAttrArgs = (ISFSPathAttrArgs*)ROUNDUP(ptr + 64);
    rc = IOS_Ioctl(__fsFd, 6, ptr, 64, pathAttrArgs, sizeof(*pathAttrArgs));

    if (rc != IOS_ERROR_OK) {
        goto out;
    }

    *ownerId = pathAttrArgs->ownerId;
    *groupId = pathAttrArgs->groupId;
    *attr = pathAttrArgs->attr;
    *ownerAcc = pathAttrArgs->ownerAccess;
    *groupAcc = pathAttrArgs->groupAccess;
    *othersAcc = pathAttrArgs->othersAccess;

out:
    if (blCtxt != 0) {
        ISFS_FREE(blCtxt);
    }

    return rc;
} // 0x80240BB0

/*
 * ISFS_GetAttrAsync
 *
 * Async GetAttr.
 */
s32 ISFS_GetAttrAsync(const u8* name, IOSUid* ownerId, IOSGid* groupId, u32* attr, u32* ownerAcc, u32* groupAcc, u32* othersAcc, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    __isfsCtxt *ctxt;
    ISFSPathAttrArgs *pathAttrArgs;
    u8 *ptr;
    u32 len;

    if (name == NULL || __fsFd < 0 ||
        (len = strnlen(name, 64)) == 64 ||
        ownerId == NULL || groupId == NULL || attr == NULL ||
        ownerAcc == NULL || groupAcc == NULL || othersAcc == NULL) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt =  iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->args.ga.ownerId = ownerId;
    ctxt->args.ga.groupId = groupId;
    ctxt->args.ga.attr = attr; 
    ctxt->args.ga.ownerAcc = ownerAcc;
    ctxt->args.ga.groupAcc = groupAcc;
    ctxt->args.ga.othersAcc = othersAcc;
    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 3;

    ptr = (u8*)ctxt->ioBuf;
    memcpy(ptr, name, len + 1);
    pathAttrArgs = (ISFSPathAttrArgs*)ROUNDUP(ptr + 64);

    rc = IOS_IoctlAsync(__fsFd, 6, ptr, 64, pathAttrArgs, sizeof(*pathAttrArgs), _isfsFuncCb, ctxt);
out:
    return rc;
} // 0x80240D10

/*
 * ISFS_Delete
 *
 * Ioctl 7: delete a file or directory.
 */
s32 ISFS_Delete(const u8* name) {
    ISFSError rc = ISFS_ERROR_OK;
    u32 len;
    __isfsCtxt* blCtxt = 0;

    if (name == NULL || __fsFd < 0 || (len = strnlen(name, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    blCtxt = iosAllocAligned(hId, sizeof(*blCtxt), 32);
    if (blCtxt == 0) {
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memcpy(blCtxt->ioBuf, name, len + 1);
    rc = IOS_Ioctl(__fsFd, 7, blCtxt->ioBuf, 64, NULL, 0);

out:
    if (blCtxt != 0) {
        ISFS_FREE(blCtxt);
    }

    return rc;
} // 0x80240E50

/*
 * ISFS_DeleteAsync
 *
 * Async Delete.
 */
s32 ISFS_DeleteAsync(const u8* name, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    u32 len;
    __isfsCtxt* ctxt;

    if (name == NULL || __fsFd < 0 || (len = strnlen(name, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    memcpy(ctxt->ioBuf, name, len + 1);
    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    rc = IOS_IoctlAsync(__fsFd, 7, ctxt->ioBuf, 64, NULL, 0, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80240F30

/*
 * ISFS_Rename
 *
 * Ioctl 8: rename a file or directory.
 */
s32 ISFS_Rename(const u8* oldName, const u8* newName) {
    ISFSError rc = ISFS_ERROR_OK;
    ISFSPathsArgs* pathsArgs;
    u32 oldLen, newLen;
    __isfsCtxt* blCtxt = 0;

    if (oldName == NULL || newName == NULL || __fsFd < 0 ||
        (oldLen = strnlen(oldName, 64)) == 64 || 
        (newLen = strnlen(newName, 64)) == 64) {
            rc = ISFS_ERROR_INVALID;
            goto out;
        }
    
    blCtxt = iosAllocAligned(hId, sizeof(*blCtxt), 32);
    if (blCtxt == 0) {
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    pathsArgs = (ISFSPathsArgs*)blCtxt->ioBuf;
    memcpy(pathsArgs->path1, oldName, oldLen + 1);
    memcpy(pathsArgs->path2, newName, newLen + 1);
    rc = IOS_Ioctl(__fsFd, 8, pathsArgs, sizeof(*pathsArgs), NULL, 0);

out:
    if (blCtxt != 0) {
        ISFS_FREE(blCtxt);
    }

    return rc;
} // 0x80241000

/*
 * ISFS_RenameAsync
 *
 * Async Rename.
 */
s32 ISFS_RenameAsync(const u8* oldName, const u8* newName, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    ISFSPathsArgs* pathsArgs;
    u32 oldLen, newLen;
    __isfsCtxt* ctxt;

    if ((oldName == NULL) || newName == NULL || __fsFd < 0 ||
        (oldLen = strnlen(oldName, 64)) == 64 || 
        (newLen = strnlen(newName, 64)) == 64) {
            rc = ISFS_ERROR_INVALID;
            goto out;
        }
    
    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == NULL) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;

    pathsArgs = (ISFSPathsArgs*)ctxt->ioBuf;
    memcpy(pathsArgs->path1, oldName, oldLen + 1);
    memcpy(pathsArgs->path2, newName, newLen + 1);
    rc = IOS_IoctlAsync(__fsFd, 8, pathsArgs, sizeof(*pathsArgs), NULL, 0, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80241110

/*
 * ISFS_GetUsage
 *
 * Ioctlv 12: blocks and inodes used under a directory.
 */
s32 ISFS_GetUsage(const u8* dname, u32* nblocks, u32* ninodes) {
    ISFSError rc = ISFS_ERROR_OK;
    IOSIoVector* v = 0;
    u32 len, *blkPtr, *inodePtr;
    char* dnPtr;
    __isfsCtxt* blCtxt = 0;

    if (dname == NULL || __fsFd < 0 ||
        nblocks == NULL || ninodes == NULL ||
        (len = strnlen(dname, 64)) == 64) {

        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    blCtxt = iosAllocAligned(hId, sizeof(*blCtxt), 32);
    if (blCtxt == 0) {
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    v = (IOSIoVector*)blCtxt->ioBuf;
    dnPtr = (char*)ROUNDUP((u8*)&v[3]);
    memcpy(dnPtr, dname, len + 1);
    v[0].base = (u8*)dnPtr;
    v[0].length = 64;

    blkPtr = (u32*) ROUNDUP(((u32)dnPtr) + 64);
    inodePtr = (u32*) ROUNDUP(((u32)blkPtr) + 4);
    v[1].base = (u8*) blkPtr;
    v[1].length = 4;
    v[2].base = (u8*)inodePtr;
    v[2].length = 4;

    rc = IOS_Ioctlv(__fsFd,  12, 1, 2, v);
    if (rc != ISFS_ERROR_OK) {
        goto out;
    }

    *nblocks = *blkPtr;
    *ninodes = *inodePtr;

out:
    if (blCtxt != 0) {
        ISFS_FREE(blCtxt);
    }

    return rc;
} // 0x80241220

/*
 * ISFS_GetUsageAsync
 *
 * Async GetUsage. Name by analogy with the other pairs (not in the reference).
 */
s32 ISFS_GetUsageAsync(const u8* dname, u32* nblocks, u32* ninodes, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    u32 len, *blkPtr, *inodePtr;
    IOSIoVector* v;
    __isfsCtxt* ctxt;
    char* dnPtr;

    if (dname == NULL || __fsFd < 0 ||
        nblocks == NULL || ninodes == NULL ||
        (len = strnlen(dname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 4;
    ctxt->args.gu.nblocks = nblocks;
    ctxt->args.gu.ninodes = ninodes;
    v = (IOSIoVector*)ctxt->ioBuf;
    dnPtr = (char*)ROUNDUP((u8*)&v[3]);
    memcpy(dnPtr, dname, len + 1);
    v[0].base = (u8*)dnPtr;
    v[0].length = 64;

    blkPtr = (u32*)ROUNDUP(((u32)dnPtr) + 64);
    inodePtr = (u32*)ROUNDUP(((u32)blkPtr) + 4);
    v[1].base = (u8*)blkPtr;
    v[1].length = 4;
    v[2].base = (u8*)inodePtr;
    v[2].length = 4;

    rc = IOS_IoctlvAsync(__fsFd, 12, 1, 2, v, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80241360

/*
 * ISFS_CreateFile
 *
 * Ioctl 9: create a file.
 */
s32 ISFS_CreateFile(const u8* fname, u32 fileAttr, u32 ownerAcc, u32 groupAcc, u32 othersAcc) {
    ISFSError rc = ISFS_ERROR_OK;
    ISFSPathAttrArgs* pathAttrArgs;
    u32 len;
    __isfsCtxt* blCtxt = NULL;

    if (fname == NULL || __fsFd < 0 || (len = strnlen(fname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    blCtxt = iosAllocAligned(hId, sizeof(*blCtxt), 32);
    if (blCtxt == 0) {
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    pathAttrArgs = (ISFSPathAttrArgs*)blCtxt->ioBuf;
    memcpy(pathAttrArgs->path, fname, len + 1);
    pathAttrArgs->attr = fileAttr;
    pathAttrArgs->ownerAccess = ownerAcc;
    pathAttrArgs->groupAccess = groupAcc;
    pathAttrArgs->othersAccess = othersAcc;
    rc = IOS_Ioctl(__fsFd, 9, pathAttrArgs, sizeof(*pathAttrArgs), NULL, 0);

out:
    if (blCtxt != 0) {
        ISFS_FREE(blCtxt);
    }

    return rc;
} // 0x80241490

/*
 * ISFS_CreateFileAsync
 *
 * Async CreateFile.
 */
s32 ISFS_CreateFileAsync(const u8* fname, u32 fileAttr, u32 ownerAcc, u32 groupAcc, u32 othersAcc, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    ISFSPathAttrArgs* pathAttrArgs;
    u32 len;
    __isfsCtxt* ctxt;

    if (fname == NULL || __fsFd < 0 || (len = strnlen(fname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;

    pathAttrArgs = (ISFSPathAttrArgs*)ctxt->ioBuf;
    memcpy(pathAttrArgs->path, fname, len + 1);
    pathAttrArgs->attr = fileAttr;
    pathAttrArgs->ownerAccess = ownerAcc;
    pathAttrArgs->groupAccess = groupAcc;
    pathAttrArgs->othersAccess = othersAcc;
    rc = IOS_IoctlAsync(__fsFd, 9, pathAttrArgs, sizeof(*pathAttrArgs), NULL, 0, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80241590

/*
 * ISFS_Open
 *
 * Opens a file through the ISFS heap (the path must live in IPC memory).
 */
IOSFd ISFS_Open(const u8* fname, u32 access) {
    ISFSError rc = ISFS_ERROR_OK;
    u32 len;
    __isfsCtxt* blCtxt = 0;

    if (fname == NULL || (len = strnlen(fname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    blCtxt = iosAllocAligned(hId, sizeof(*blCtxt), 32);
    if (blCtxt == 0) {
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memcpy(blCtxt->ioBuf, fname, len + 1);
    rc = IOS_Open((const char*)blCtxt->ioBuf, access);

out:
    if (blCtxt != 0) {
        ISFS_FREE(blCtxt);
    }

    return rc;
} // 0x80241690

/*
 * ISFS_OpenAsync
 *
 * Async Open.
 */
IOSFd ISFS_OpenAsync(const u8* fname, u32 access, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    u32 len;
    __isfsCtxt* ctxt;

    if (fname == NULL || (len = strnlen(fname, 64)) == 64) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    memcpy(ctxt->ioBuf, fname, len + 1);
    rc = IOS_OpenAsync((const char*)ctxt->ioBuf, access, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80241760

/*
 * ISFS_SeekAsync
 *
 * Async seek on an open file.
 */
s32 ISFS_SeekAsync(IOSFd fd, s32 offset, u32 whence, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc;
    __isfsCtxt* ctxt;

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    rc = IOS_SeekAsync(fd, offset, whence, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80241820

/*
 * ISFS_Read
 *
 * Reads into a 32-byte aligned buffer.
 */
s32 ISFS_Read(s32 fd, u8* pBuffer, u32 bufSize) {
    ISFSError rc = ISFS_ERROR_OK;

    /* nullptr check and alignment to 0x20 */
    if (pBuffer == NULL || (u32)pBuffer & 31) {
        rc = ISFS_ERROR_INVALID;
    } else {
        rc = IOS_Read(fd, pBuffer, bufSize);
    }

    return rc;
} // 0x802418B0

/*
 * ISFS_ReadAsync
 *
 * Async Read.
 */
s32 ISFS_ReadAsync(IOSFd fd, u8* buf, u32 size, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    __isfsCtxt* ctxt;

    if (buf == NULL || ((u32)buf & 31)) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    rc = IOS_ReadAsync(fd, buf, size, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x802418D0

/*
 * ISFS_Write
 *
 * Writes from a 32-byte aligned buffer.
 */
s32 ISFS_Write(IOSFd fd, const u8* buf, u32 size) {
    ISFSError rc = ISFS_ERROR_OK;

    if (buf == NULL || ((u32)buf & 31)) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    rc = IOS_Write(fd, (void*)buf, size);
out:
    return rc;
} // 0x80241980

/*
 * ISFS_WriteAsync
 *
 * Async Write.
 */
s32 ISFS_WriteAsync(IOSFd fd, const u8* buf, u32 size, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc = ISFS_ERROR_OK;
    __isfsCtxt* ctxt;

    if (buf == NULL || ((u32)buf & (u32)(31))) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);
    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    rc = IOS_WriteAsync(fd, (void*)buf, size, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x802419A0

/*
 * ISFS_Close
 *
 * Closes a file.
 */
s32 ISFS_Close(IOSFd fd) {
    ISFSError rc = ISFS_ERROR_OK;
    rc = IOS_Close(fd);
    return rc;
} // 0x80241A50

/*
 * ISFS_CloseAsync
 *
 * Async Close.
 */
s32 ISFS_CloseAsync(IOSFd fd, ISFSCallback cb, void* fsCtxt) {
    ISFSError rc;
    __isfsCtxt* ctxt;

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);

    if (ctxt == 0) {
        rc = ISFS_ERROR_BUSY;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    rc = IOS_CloseAsync(fd, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80241A60

/*
 * ISFS_ShutdownAsync
 *
 * Ioctl 13: asks /dev/fs to shut down.
 */
s32 ISFS_ShutdownAsync(ISFSCallback cb, void* fsCtxt) {
    __isfsCtxt* ctxt;
    s32 rc = ISFS_ERROR_OK;

    ctxt = iosAllocAligned(hId, sizeof(*ctxt), 32);

    if (__fsFd < 0) {
        rc = ISFS_ERROR_INVALID;
        goto out;
    }

    ctxt->cb = cb;
    ctxt->ctxt = fsCtxt;
    ctxt->func = 0;
    rc = IOS_IoctlAsync(__fsFd, 13, NULL, 0, NULL, 0, _isfsFuncCb, ctxt);

out:
    return rc;
} // 0x80241AF0

/*
 * _FSGetStatsCb
 *
 * Copies the ISFSStats result out (inlined into _isfsFuncCb; nothing starts that request in this build).
 */
static IOSError _FSGetStatsCb(IOSError ret, void* ctxt) {
    __isfsCtxt* _ctxt = (__isfsCtxt*)ctxt;
    ISFSError rc = ISFS_ERROR_OK;

    if (ret == 0) {
        memcpy(_ctxt->args.stats, _ctxt->ioBuf, sizeof(*_ctxt->args.stats));
    }

    return rc;
}

/*
 * _FSReadDirCb
 *
 * Copies the entry count out (inlined into _isfsFuncCb).
 */
static IOSError _FSReadDirCb(IOSError ret, void* ctxt) {
    ISFSError rc = ISFS_ERROR_OK;
    __isfsCtxt* _ctxt = (__isfsCtxt*)ctxt;

    if (ret == 0) {
        u8* ptr;
        IOSIoVector* v = (IOSIoVector*)_ctxt->ioBuf;
        ptr = (u8*)ROUNDUP((u8*)&v[4]);
        ptr = (u8*)ROUNDUP(ptr + 64);
        *_ctxt->args.num = *(u32 *) ptr;
    }

    return rc;
}

/*
 * _FSGetAttrCb
 *
 * Copies the six attribute fields out (inlined into _isfsFuncCb).
 */
static IOSError _FSGetAttrCb(IOSError ret, void* ctxt) {
    ISFSError rc = ret;

    if (ret == 0) {
        __isfsCtxt* _ctxt = (__isfsCtxt*)ctxt;
        ISFSPathAttrArgs* pathAttrArgs = (ISFSPathAttrArgs*)ROUNDUP(_ctxt->ioBuf + 64);
    
        *_ctxt->args.ga.ownerId = pathAttrArgs->ownerId;
        *_ctxt->args.ga.groupId = pathAttrArgs->groupId;
        *_ctxt->args.ga.attr = pathAttrArgs->attr; 
        *_ctxt->args.ga.ownerAcc = pathAttrArgs->ownerAccess;
        *_ctxt->args.ga.groupAcc = pathAttrArgs->groupAccess;
        *_ctxt->args.ga.othersAcc = pathAttrArgs->othersAccess;
    }

    return rc;
}

/*
 * _FSGetUsageCb
 *
 * Copies block and inode counts out (inlined into _isfsFuncCb).
 */
static IOSError _FSGetUsageCb(IOSError ret, void* ctxt) {
    ISFSError rc = ISFS_ERROR_OK;
    __isfsCtxt* _ctxt = (__isfsCtxt*)ctxt;

    if (ret == 0) {
        u8* ptr;
        IOSIoVector* v = (IOSIoVector*)_ctxt->ioBuf;
        ptr = (u8*)ROUNDUP((u8*)&v[4]);
        ptr = (u8*)ROUNDUP(ptr + 64);
        *_ctxt->args.gu.nblocks = *(u32*)ptr;
        ptr = (u8*)ROUNDUP(ptr + 4);
        *_ctxt->args.gu.ninodes = *(u32*)ptr;
    }

    return rc;
}

/*
 * _FSGetFileStatsCb
 *
 * Copies the ISFSFileStats result out (inlined into _isfsFuncCb).
 */
static IOSError _FSGetFileStatsCb(IOSError ret, void* ctxt) {
    __isfsCtxt* _ctxt = (__isfsCtxt*)ctxt;
    ISFSError rc = ISFS_ERROR_OK;

    if (ret == 0) {
        memcpy(_ctxt->args.fstats, _ctxt->ioBuf, sizeof(*_ctxt->args.fstats));
    }

    return rc;
}
