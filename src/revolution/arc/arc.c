// Reference: zeldaret/tp libs/revolution/src/arc/arc.c
//
// ARCOpen is not in the reference (retail reports a missing file with the current directory, like DVDOpen in
// dvdfs.c). ARCGetCurrentDir, ARCConvertEntrynumToPath and myStrncpy have no address of their own: they are
// inlined into ARCOpen and entryToPath.
#include <ctype.h>
#include <revolution/arc.h>
#include <revolution/os.h>
#include <revolution/dvd.h>

#define entryIsDir(fstStart, i) ((((fstStart)[(i)].isDirAndStringOff & 0xFF000000) == 0) ? FALSE : TRUE)
#define stringOff(fstStart, i) ((fstStart)[(i)].isDirAndStringOff & 0x00FFFFFF)
#define parentDir(fstStart, i) ((fstStart)[(i)].parentOrPosition)
#define nextDir(fstStart, i) ((fstStart)[(i)].nextEntryOrLength)
#define filePosition(fstStart, i) ((fstStart)[(i)].parentOrPosition)
#define fileLength(fstStart, i) ((fstStart)[(i)].nextEntryOrLength)

static BOOL isSame(const char* path, const char* string);
static u32 myStrncpy(char* dest, char* src, u32 maxlen);
static u32 entryToPath(ARCHandle* handle, u32 entry, char* path, u32 maxlen);
static BOOL ARCConvertEntrynumToPath(ARCHandle* handle, s32 entrynum, char* path, u32 maxlen);
static BOOL ARCGetCurrentDir(ARCHandle* handle, char* path, u32 maxlen);

/*
 * ARCInitHandle
 *
 * Locates the FST and the file data of an archive in memory. The root entry's
 * length field is the entry count; the name strings follow the entries.
 */
BOOL ARCInitHandle(void* arcStart, ARCHandle* handle) {
    FSTEntry* FSTEntries;
    ARCHeader* arcHeader = (ARCHeader*)arcStart;

    if (arcHeader->magic != ARC_MAGIC) {
        OSPanic(__FILE__, 74, "ARCInitHandle: bad archive format");
    }

    handle->archiveStartAddr = arcStart;
    handle->FSTStart = FSTEntries = (void*)((u32)arcStart + arcHeader->fstStart);
    handle->fileStart = (void*)((u32)arcStart + arcHeader->fileStart);
    handle->entryNum = nextDir(FSTEntries, 0);
    handle->FSTStringStart = (char*)&(FSTEntries[handle->entryNum]);
    handle->FSTLength = (u32)arcHeader->fstSize;
    handle->currDir = 0;
    return TRUE;
} // 0x80249DB0

/*
 * ARCOpen
 *
 * Opens a file by path. A path that is not found is reported with the current
 * directory.
 */
BOOL ARCOpen(ARCHandle* handle, const char* fileName, ARCFileInfo* af) {
    s32 entry;
    char currentDir[128];
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;

    entry = ARCConvertPathToEntrynum(handle, fileName);

    if (0 > entry) {
        ARCGetCurrentDir(handle, currentDir, 128);
        OSReport("Warning: ARCOpen(): file '%s' was not found under %s in the archive.\n", fileName, currentDir);
        return FALSE;
    }

    if (entryIsDir(FSTEntries, entry)) {
        return FALSE;
    }

    af->handle = handle;
    af->startOffset = filePosition(FSTEntries, entry);
    af->length = fileLength(FSTEntries, entry);
    return TRUE;
} // 0x80249E50

/*
 * ARCFastOpen
 *
 * Fills a file info block from an FST entry. Fails for an entry number out of
 * range and for a directory.
 */
BOOL ARCFastOpen(ARCHandle* handle, s32 entrynum, ARCFileInfo* af) {
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;

    if ((entrynum < 0) || (entrynum >= handle->entryNum) || entryIsDir(FSTEntries, entrynum)) {
        return FALSE;
    }

    af->handle = handle;
    af->startOffset = filePosition(FSTEntries, entrynum);
    af->length = fileLength(FSTEntries, entrynum);
    return TRUE;
} // 0x8024A100

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

    if ((*path == '/') || (*path == '\0')) {
        return TRUE;
    }

    return FALSE;
}

/*
 * ARCConvertPathToEntrynum
 *
 * Resolves a path, relative to the current directory unless it starts with
 * '/', to an FST entry number. Returns -1 when a component is not found.
 */
s32 ARCConvertPathToEntrynum(ARCHandle* handle, const char* pathPtr) {
    const char* ptr;
    char* stringPtr;
    BOOL isDir;
    s32 length;
    u32 dirLookAt;
    u32 i;
    FSTEntry* FSTEntries;

    dirLookAt = handle->currDir;
    FSTEntries = (FSTEntry*)handle->FSTStart;

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
                    dirLookAt = parentDir(FSTEntries, dirLookAt);
                    pathPtr += 3;
                    continue;
                } else if (*(pathPtr + 2) == '\0') {
                    return (s32)parentDir(FSTEntries, dirLookAt);
                }
            } else if (*(pathPtr + 1) == '/') {
                pathPtr += 2;
                continue;
            } else if (*(pathPtr + 1) == '\0') {
                return (s32)dirLookAt;
            }
        }

        for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++)
            ;
        isDir = (*ptr == '\0') ? FALSE : TRUE;
        length = (s32)(ptr - pathPtr);
        ptr = pathPtr;

        for (i = dirLookAt + 1; i < nextDir(FSTEntries, dirLookAt);
             i = entryIsDir(FSTEntries, i) ? nextDir(FSTEntries, i) : (i + 1)) {
dot:
            if ((entryIsDir(FSTEntries, i) == FALSE) && (isDir == TRUE)) {
                continue;
            }

            stringPtr = handle->FSTStringStart + stringOff(FSTEntries, i);

            if (*stringPtr == '.' && *(stringPtr + 1) == '\0') {
                i++;
                goto dot;
            }

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
} // 0x8024A150

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
static u32 entryToPath(ARCHandle* handle, u32 entry, char* path, u32 maxlen) {
    char* name;
    u32 loc;
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;

    if (entry == 0) {
        return 0;
    }

    name = handle->FSTStringStart + stringOff(FSTEntries, entry);
    loc = entryToPath(handle, parentDir(FSTEntries, entry), path, maxlen);

    if (loc == maxlen) {
        return loc;
    }

    *(path + loc++) = '/';
    loc += myStrncpy(path + loc, name, maxlen - loc);
    return loc;
} // 0x8024A3D0

/*
 * ARCConvertEntrynumToPath
 *
 * Writes the path of an FST entry into path. A directory's path ends in '/'.
 */
static BOOL ARCConvertEntrynumToPath(ARCHandle* handle, s32 entrynum, char* path, u32 maxlen) {
    u32 loc;
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;

    loc = entryToPath(handle, (u32)entrynum, path, maxlen);

    if (loc == maxlen) {
        path[maxlen - 1] = '\0';
        return FALSE;
    }

    if (entryIsDir(FSTEntries, entrynum)) {
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
 * ARCGetCurrentDir
 *
 * Writes the path of the archive's current directory into path.
 */
static BOOL ARCGetCurrentDir(ARCHandle* handle, char* path, u32 maxlen) {
    return ARCConvertEntrynumToPath(handle, (s32)handle->currDir, path, maxlen);
}

/*
 * ARCGetStartAddrInMem
 *
 * Returns the address of a file's data inside the archive image.
 */
void* ARCGetStartAddrInMem(ARCFileInfo* af) {
    ARCHandle* handle = af->handle;
    return (void*)((u32)handle->archiveStartAddr + af->startOffset);
} // 0x8024A5A0

/*
 * ARCGetLength
 *
 * Returns the length of an open file.
 */
u32 ARCGetLength(ARCFileInfo* af) {
    return af->length;
} // 0x8024A5C0

/*
 * ARCClose
 *
 * Nothing to release for an archive file.
 */
BOOL ARCClose(ARCFileInfo* af) {
    return TRUE;
} // 0x8024A5D0

/*
 * ARCChangeDir
 *
 * Makes a directory the archive's current directory.
 */
BOOL ARCChangeDir(ARCHandle* handle, const char* dirName) {
    s32 entry;
    FSTEntry* FSTEntries;

    entry = ARCConvertPathToEntrynum(handle, dirName);
    FSTEntries = (FSTEntry*)handle->FSTStart;

    if ((entry < 0) || (entryIsDir(FSTEntries, entry) == FALSE)) {
        return FALSE;
    }

    handle->currDir = (u32)entry;
    return TRUE;
} // 0x8024A5E0

/*
 * ARCOpenDir
 *
 * Starts a directory listing: location is the first entry after the directory,
 * next the first entry past its last child.
 */
BOOL ARCOpenDir(ARCHandle* handle, const char* dirName, ARCDir* dir) {
    s32 entry;
    FSTEntry* FSTEntries;

    entry = ARCConvertPathToEntrynum(handle, dirName);
    FSTEntries = (FSTEntry*)handle->FSTStart;

    if ((entry < 0) || (entryIsDir(FSTEntries, entry) == FALSE)) {
        return FALSE;
    }

    dir->handle = handle;
    dir->entryNum = (u32)entry;
    dir->location = (u32)entry + 1;
    dir->next = nextDir(FSTEntries, entry);
    return TRUE;
} // 0x8024A640

/*
 * ARCReadDir
 *
 * Returns the next entry of a directory listing, skipping "." entries.
 */
BOOL ARCReadDir(ARCDir* dir, ARCDirEntry* dirent) {
    u32 loc;
    FSTEntry* FSTEntries;
    ARCHandle* handle;

    handle = dir->handle;
    FSTEntries = (FSTEntry*)handle->FSTStart;
    loc = dir->location;
retry:
    if ((loc <= dir->entryNum) || (dir->next <= loc)) {
        return FALSE;
    }

    dirent->handle = handle;
    dirent->entryNum = loc;
    dirent->isDir = entryIsDir(FSTEntries, loc);
    dirent->name = handle->FSTStringStart + stringOff(FSTEntries, loc);

    if (dirent->name[0] == '.' && dirent->name[1] == '\0') {
        loc++;
        goto retry;
    }

    dir->location = entryIsDir(FSTEntries, loc) ? nextDir(FSTEntries, loc) : (loc + 1);
    return TRUE;
} // 0x8024A6C0

/*
 * ARCCloseDir
 *
 * Nothing to release for a directory listing.
 */
BOOL ARCCloseDir(ARCDir* dir) {
    return TRUE;
} // 0x8024A780
