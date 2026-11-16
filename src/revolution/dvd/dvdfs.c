// Reference: zeldaret/tp libs/revolution/src/dvd/dvdfs.c
// Reference: doldecomp/ogws src/revolution/DVD/dvdfs.c (entryToPath, DVDReadAsyncPrio, cbForReadAsync)
//
// The debug-only checks (ASSERTMSGLINE, ASSERTLINE, ASSERTMSG1LINE, ASSERTMSG2LINE) compile to nothing in this
// release build and are left out. DVD_ASSERTMSGLINE checks stay, with the retail build's line numbers.
// Functions nothing in the game calls (DVDChangeDir, DVDReadPrio, ...) are kept in reference order. The linker
// dead-strips them, so they have no retail address and are not checked against retail bytes. DVDGetCurrentDir,
// DVDConvertEntrynumToPath and myStrncpy have no address of their own either: the first two are inlined into DVDOpen,
// myStrncpy into entryToPath.
#include <ctype.h>
#include <revolution/hardware.h>
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>

static OSBootInfo* BootInfo;
static FSTEntry* FstStart;
static char* FstStringStart;
static u32 MaxEntryNum;
OSThreadQueue __DVDThreadQueue;
static u32 currentDirectory;

u32 __DVDLongFileNameFlag = 1;

// prototypes
static BOOL isSame(const char* path, const char* string);
static u32 myStrncpy(char* dest, char* src, u32 maxlen);
static u32 entryToPath(u32 entry, char* path, u32 maxlen);
static BOOL DVDConvertEntrynumToPath(s32 entrynum, char* path, u32 maxlen);
static void cbForReadAsync(s32 result, DVDCommandBlock* block);
static void cbForReadSync(s32 result, DVDCommandBlock* block);
static void cbForSeekAsync(s32 result, DVDCommandBlock* block);
static void cbForSeekSync(s32 result, DVDCommandBlock* block);

/*
 * __DVDFSInit
 *
 * Reads the FST location from the boot info. The root entry's length field
 * is the entry count; the name strings follow the entries.
 */
void __DVDFSInit(void) {
    BootInfo = (void*)OSPhysicalToCached(0);
    FstStart = BootInfo->FSTLocation;
    if (FstStart) {
        MaxEntryNum = FstStart->nextEntryOrLength;
        FstStringStart = (char*)FstStart + (MaxEntryNum* sizeof(FSTEntry));
    }
} // 0x8021EFA0

/* For convenience */
#define entryIsDir(i) (((FstStart[(i)].isDirAndStringOff & 0xff000000) == 0) ? FALSE : TRUE)
#define stringOff(i) (FstStart[(i)].isDirAndStringOff & ~0xff000000)
#define parentDir(i) (FstStart[(i)].parentOrPosition)
#define nextDir(i) (FstStart[(i)].nextEntryOrLength)
#define filePosition(i) (FstStart[(i)].parentOrPosition)
#define fileLength(i) (FstStart[(i)].nextEntryOrLength)

/*
 * isSame
 *
 * Tests whether a path component matches an entry name, ignoring case. The
 * component must end at a '/' or at the end of the path.
 */
static BOOL isSame(const char* path, const char* string) {
    while (*string != '\0') {
        if (tolower(*path++) != tolower(*string++)) {
            return FALSE;
        }
    }

    if (*path == '/' || *path == '\0') {
        return TRUE;
    }

    return FALSE;
}

/*
 * DVDConvertPathToEntrynum
 *
 * Resolves a path, relative to the current directory unless it starts with
 * '/', to an FST entry number. Returns -1 when a component is not found.
 */
s32 DVDConvertPathToEntrynum(const char* pathPtr) {
    const char* ptr;
    char* stringPtr;
    BOOL isDir;
    u32 length;
    u32 dirLookAt;
    u32 i;
    const char* origPathPtr = pathPtr;
    const char* extentionStart;
    BOOL illegal;
    BOOL extention;

    dirLookAt = currentDirectory;

    while (1) {
        if (*pathPtr == '\0') {
            return (s32)dirLookAt;
        } else if (*pathPtr == '/') {
            dirLookAt = 0;
            pathPtr++;
            continue;
        } else if (*pathPtr == '.') {
            if (*(pathPtr + 1) == '.') {
                if (*(pathPtr + 2) == '/') {
                    dirLookAt = parentDir(dirLookAt);
                    pathPtr += 3;
                    continue;
                } else if (*(pathPtr + 2) == '\0') {
                    return (s32)parentDir(dirLookAt);
                }
            } else if (*(pathPtr + 1) == '/') {
                pathPtr += 2;
                continue;
            } else if (*(pathPtr + 1) == '\0') {
                return (s32)dirLookAt;
            }
        }

        if (__DVDLongFileNameFlag == 0) {
            extention = FALSE;
            illegal = FALSE;

            for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++) {
                if (*ptr == '.') {
                    if ((ptr - pathPtr > 8) || (extention == TRUE)) {
                        illegal = TRUE;
                        break;
                    }
                    extention = TRUE;
                    extentionStart = ptr + 1;

                } else if (*ptr == ' ') {
                    illegal = TRUE;
                }
            }
            if ((extention == TRUE) && (ptr - extentionStart > 3)) {
                illegal = TRUE;
            }

            if (illegal) {
                OSPanic(__FILE__, 443,
                    "DVDConvertEntrynumToPath(possibly DVDOpen or DVDChangeDir or DVDOpenDir): "
                    "specified directory or file (%s) doesn't match standard 8.3 format. This is a "
                    "temporary restriction and will be removed soon\n",
                    origPathPtr);
            }
        } else {
            for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++)
                ;
        }

        isDir = (*ptr == '\0') ? FALSE : TRUE;
        length = (u32)(ptr - pathPtr);

        ptr = pathPtr;

        for (i = dirLookAt + 1; i < nextDir(dirLookAt); i = entryIsDir(i) ? nextDir(i) : (i + 1)) {
            if ((entryIsDir(i) == FALSE) && (isDir == TRUE)) {
                continue;
            }

            stringPtr = FstStringStart + stringOff(i);

            if (isSame(ptr, stringPtr) == TRUE) {
                goto next_hier;
            }
        }

        return -1;

next_hier:
        if (!isDir) {
            return (s32)i;
        }

        dirLookAt = i;
        pathPtr += length + 1;
    }
} // 0x8021EFD0

/*
 * DVDFastOpen
 *
 * Fills a file info block from an FST entry. Fails for an entry number out of
 * range and for a directory.
 */
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo) {
    if (entrynum < 0 || entrynum >= MaxEntryNum || entryIsDir(entrynum)) {
        return FALSE;
    }

    fileInfo->startAddr = filePosition(entrynum) >> __DVDLayoutFormat;
    fileInfo->length = fileLength(entrynum);
    fileInfo->callback = (DVDCallback)NULL;
    fileInfo->cb.state = DVD_STATE_END;

    return TRUE;
} // 0x8021F2E0

/*
 * DVDOpen
 *
 * Opens a file by path. A path that is not found is reported with the current
 * directory.
 */
BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo) {
    s32 entry;
    char currentDir[128];

    entry = DVDConvertPathToEntrynum(fileName);

    if (0 > entry) {
        DVDGetCurrentDir(currentDir, 128);
        OSReport("Warning: DVDOpen(): file '%s' was not found under %s.\n", fileName, currentDir);
        return FALSE;
    }

    if (entryIsDir(entry)) {
        return FALSE;
    }

    fileInfo->startAddr = filePosition(entry) >> __DVDLayoutFormat;
    fileInfo->length = fileLength(entry);
    fileInfo->callback = (DVDCallback)NULL;
    fileInfo->cb.state = DVD_STATE_END;

    return TRUE;
} // 0x8021F350

/*
 * DVDClose
 *
 * Cancels any transfer on the file.
 */
BOOL DVDClose(DVDFileInfo* fileInfo) {
    DVDCancel(&(fileInfo->cb));
    return TRUE;
} // 0x8021F610

/*
 * myStrncpy
 *
 * Copies at most maxlen characters of a NUL-terminated string and returns how
 * many were copied.
 */
static u32 myStrncpy(char* dest, char* src, u32 maxlen) {
    u32 i = maxlen;

    while ((i > 0) && (*src != 0)) {
        *dest++ = *src++;
        i--;
    }

    return (maxlen - i);
}

/*
 * entryToPath
 *
 * Writes the path of a directory entry into path, parent first, and returns
 * the length written. Returns maxlen when the path does not fit.
 */
static u32 entryToPath(u32 entry, char* path, u32 maxlen) {
    char* name;
    u32 loc;

    if (entry == 0) {
        return 0;
    }

    name = FstStringStart + stringOff(entry);

    loc = entryToPath(parentDir(entry), path, maxlen);
    if (loc == maxlen) {
        return loc;
    }

    *(path + loc++) = '/';

    loc += myStrncpy(path + loc, name, maxlen - loc);
    return loc;
} // 0x8021F640

/*
 * DVDConvertEntrynumToPath
 *
 * Writes the path of an FST entry into path. A directory's path ends in '/'.
 */
static BOOL DVDConvertEntrynumToPath(s32 entrynum, char* path, u32 maxlen) {
    u32 loc;

    loc = entryToPath((u32)entrynum, path, maxlen);

    if (loc == maxlen) {
        path[maxlen - 1] = '\0';
        return FALSE;
    }

    if (entryIsDir(entrynum)) {
        if (loc == maxlen - 1) {
            path[loc] = '\0';
            return FALSE;
        }

        path[loc++] = '/';
    }

    path[loc] = '\0';
    return TRUE;
}

/*
 * DVDGetCurrentDir
 *
 * Writes the path of the current directory into path.
 */
BOOL DVDGetCurrentDir(char* path, u32 maxlen) {
    return DVDConvertEntrynumToPath((s32)currentDirectory, path, maxlen);
}

/*
 * DVDChangeDir
 *
 * Makes a directory the current directory. Fails for a path that is not a
 * directory.
 */
BOOL DVDChangeDir(const char* dirName) {
    s32 entry;

    entry = DVDConvertPathToEntrynum(dirName);
    if (entry < 0 || entryIsDir(entry) == FALSE) {
        return FALSE;
    }

    currentDirectory = (u32)entry;
    return TRUE;
}

/*
 * DVDReadAsyncPrio
 *
 * Starts an asynchronous read of length bytes at offset in the file. The
 * file's callback runs when the transfer completes.
 */
BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback, s32 prio) {
    DVD_ASSERTMSGLINE(823, (0 <= offset) && (offset <= fileInfo->length), "DVDReadAsync(): specified area is out of the file  ");

    DVD_ASSERTMSGLINE(829, (0 <= offset + length) && (offset + length < fileInfo->length + DVD_MIN_TRANSFER_SIZE), "DVDReadAsync(): specified area is out of the file  ");

    fileInfo->callback = callback;
    DVDReadAbsAsyncPrio(&(fileInfo->cb), addr, length, (s32)(fileInfo->startAddr + (offset >> 2)),
                        cbForReadAsync, prio);
    return TRUE;
} // 0x8021F810

/*
 * cbForReadAsync
 *
 * Completion callback of DVDReadAsyncPrio: passes the result to the file's callback.
 */
static void cbForReadAsync(s32 result, DVDCommandBlock* block) {
    DVDFileInfo* fileInfo = (DVDFileInfo*)block; // cb is the first member of DVDFileInfo

    if (fileInfo->callback) {
        (fileInfo->callback)(result, fileInfo);
    }
} // 0x8021F8E0

/*
 * DVDReadPrio
 *
 * Synchronous read: waits on __DVDThreadQueue until the transfer completes.
 * Returns the bytes transferred, or a negative value on error.
 */
s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio) {
    int result;
    DVDCommandBlock* block;
    s32 state;
    BOOL enabled;
    s32 retVal;

    DVD_ASSERTMSGLINE(915, (0 <= offset) && (offset <= fileInfo->length), "DVDRead(): specified area is out of the file  ");
    DVD_ASSERTMSGLINE(921, (0 <= offset + length) && (offset + length < fileInfo->length + DVD_MIN_TRANSFER_SIZE), "DVDRead(): specified area is out of the file  ");

    block = &fileInfo->cb;
    result = DVDReadAbsAsyncPrio(block, addr, length, fileInfo->startAddr + (offset >> 2), cbForReadSync, prio);
    if (result == 0) {
        return -1;
    }
    enabled = OSDisableInterrupts();

    while (1) {
        state = ((volatile DVDCommandBlock*)block)->state; // volatile: the loop re-reads the state that the transfer's callback changes
        if (state == 0) {
            retVal = (s32)block->transferredSize;
            break;
        } else if (state == -1) {
            retVal = -1;
            break;
        } else if (state == 10) {
            retVal = -3;
            break;
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

/*
 * cbForReadSync
 *
 * Completion callback of DVDReadPrio: wakes the waiting thread.
 */
static void cbForReadSync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
}

/*
 * DVDSeekAsyncPrio
 *
 * Starts an asynchronous seek to offset in the file.
 */
int DVDSeekAsyncPrio(DVDFileInfo* fileInfo, s32 offset, DVDCallback callback, s32 prio) {
    DVD_ASSERTMSGLINE(907, (0 <= offset) && (offset <= fileInfo->length), "DVDSeek(): offset is out of the file  ");

    fileInfo->callback = callback;
    DVDSeekAbsAsyncPrio(&fileInfo->cb, (u32)(char*)fileInfo->startAddr + offset, cbForSeekAsync, prio);
    return 1;
}

/*
 * cbForSeekAsync
 *
 * Completion callback of DVDSeekAsyncPrio: passes the result to the file's callback.
 */
static void cbForSeekAsync(s32 result, DVDCommandBlock* block) {
    DVDFileInfo* fileInfo;

    fileInfo = (DVDFileInfo*)&block->next;
    if (fileInfo->callback) {
        (fileInfo->callback)(result, fileInfo);
    }
}

/*
 * DVDSeekPrio
 *
 * Synchronous seek: waits on __DVDThreadQueue until the seek completes.
 */
s32 DVDSeekPrio(DVDFileInfo* fileInfo, s32 offset, s32 prio) {
    int result;
    DVDCommandBlock* block;
    s32 state;
    BOOL enabled;
    s32 retVal;

    block = &fileInfo->cb;
    result = DVDSeekAbsAsyncPrio(block, (u32)(char*)fileInfo->startAddr + offset, cbForSeekSync, prio);
    if (!result) {
        return -1;
    }
    enabled = OSDisableInterrupts();

    while (1) {
        state = ((volatile DVDCommandBlock*)block)->state; // volatile: the loop re-reads the state that the seek's callback changes
        if (state == 0) {
            retVal = 0;
            break;
        } else if (state == -1) {
            retVal = -1;
            break;
        } else if (state == 10) {
            retVal = -3;
            break;
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return retVal;
}

/*
 * cbForSeekSync
 *
 * Completion callback of DVDSeekPrio: wakes the waiting thread.
 */
static void cbForSeekSync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
}

/*
 * DVDGetFileInfoStatus
 *
 * Returns the state of the file's command block.
 */
s32 DVDGetFileInfoStatus(const DVDFileInfo* fileInfo) {
    return DVDGetCommandBlockStatus(&fileInfo->cb);
}

/*
 * DVDFastOpenDir
 *
 * Fills a directory handle from an FST entry. Fails for an entry number out of
 * range and for a file.
 */
BOOL DVDFastOpenDir(s32 entrynum, DVDDir* dir) {
    if (entrynum < 0 || entrynum >= MaxEntryNum || !entryIsDir(entrynum)) {
        return FALSE;
    }

    dir->entryNum = entrynum;
    dir->location = entrynum + 1;
    dir->next = FstStart[entrynum].nextEntryOrLength;
    return TRUE;
}

/*
 * DVDOpenDir
 *
 * Opens a directory by path. A path that is not found is reported with the
 * current directory.
 */
BOOL DVDOpenDir(const char* dirName, DVDDir* dir) {
    s32 entry;
    char currentDir[128];

    entry = DVDConvertPathToEntrynum(dirName);
    if (entry < 0) {
        DVDGetCurrentDir(currentDir, sizeof(currentDir));
        OSReport("Warning: DVDOpenDir(): file \'%s\' was not found under %s.\n", dirName, currentDir);
        return FALSE;
    }

    if (!entryIsDir(entry)) {
        return FALSE;
    }

    dir->entryNum = entry;
    dir->location = entry + 1;
    dir->next = nextDir(entry);
    return TRUE;
}

/*
 * DVDReadDir
 *
 * Returns the next entry of an open directory. Returns 0 at the end.
 */
int DVDReadDir(DVDDir* dir, DVDDirEntry* dirent) {
    u32 loc;

    loc = dir->location;
    if ((loc <= (u32) dir->entryNum) || ((u32) dir->next <= loc)) {
        return 0;
    }
    dirent->entryNum = loc;
    dirent->isDir = entryIsDir(loc);
    dirent->name = FstStringStart + stringOff(loc);
    dir->location = entryIsDir(loc) ? nextDir(loc) : loc + 1;
    return 1;
}

/*
 * DVDCloseDir
 *
 * Closes a directory handle; nothing is held open.
 */
int DVDCloseDir(DVDDir* dir) {
    return 1;
}

/*
 * DVDRewindDir
 *
 * Rewinds an open directory to its first entry.
 */
void DVDRewindDir(DVDDir* dir) {
    dir->location = dir->entryNum + 1;
}

/*
 * DVDGetFSTLocation
 *
 * Returns the FST location from the boot info.
 */
void* DVDGetFSTLocation(void) {
    return BootInfo->FSTLocation;
}

/*
 * DVDGetTransferredSize
 *
 * Returns the number of bytes the file's current transfer has moved.
 */
s32 DVDGetTransferredSize(DVDFileInfo* fileinfo) {
    s32 bytes;
    DVDCommandBlock* cb;

    cb = &(fileinfo->cb);

    switch (cb->state) {
    case DVD_STATE_COVER_CLOSED:
    case DVD_STATE_NO_DISK:
    case DVD_STATE_COVER_OPEN:
    case DVD_STATE_WRONG_DISK:
    case DVD_STATE_FATAL_ERROR:
    case DVD_STATE_MOTOR_STOPPED:
    case DVD_STATE_CANCELED:
    case DVD_STATE_RETRY:
    case DVD_STATE_END:
        bytes = (s32)cb->transferredSize;
        break;
    case DVD_STATE_WAITING:
        bytes = 0;
        break;
    case DVD_STATE_BUSY:
        bytes = (s32)(cb->transferredSize + (cb->currTransferSize - __DIRegs[6]));
        break;
    default:
        break;
    }

    return bytes;
}

/*
 * DVDGetEntrynum
 *
 * Stub kept from the reference; it has no retail address.
 */
void DVDGetEntrynum(void) {
    OSReport("DVDGetEntrynum(): null pointer is specified to file info address  ");
}

/*
 * DVDGetEntryName
 *
 * Stub kept from the reference; it has no retail address.
 */
void DVDGetEntryName(void) {
    // quirk: the reference's format has a %d and passes no argument; kept as written
    OSReport("DVDGetEntryName: specified entrynum(%d) is out of range  ");
}
