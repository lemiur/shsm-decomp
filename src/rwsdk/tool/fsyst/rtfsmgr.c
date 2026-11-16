// Unit name and boundary: Gormiti link map (RpFSyst.a rtfsmgr.obj)
// Names and layout: Gormiti link map and DWARF (RpFSyst.a rtfsmgr.obj), Final and Debug builds.
/*
 * rtfsmgr.c: the RtFileSystem manager: it keeps the registered file systems, finds the one a
 * path names, and carries the stdio-style RwF* calls the core's streams use.
 *
 * Statics of the unit (Gormiti link map): .bss `fsm` (the manager record), .sbss
 * `defaultFileSystem`; the five .sdata strings are the access-mode letters RwFopen looks for.
 */
#include <string.h>
#include <stdio.h>
#include <revolution/os/OSInterrupt.h>
#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bafsys.h>
#include <rwsdk/tool/fsyst/rtfsyst.h>
#include <rwsdk/tool/fsyst/rtfsmgr.h>

static RtFSManager fsm;
static RtFileSystem *defaultFileSystem;

// Passes an error to the manager's error call-back, when one is set.
// no copy in retail (inlined)
static void TkFSManagerTriggerErrorCallBack(RtFSManagerError err) {
    if (fsm.CallBacks.errorCallBack != NULL) {
        fsm.CallBacks.errorCallBack(err);
    }
}

// Starts the manager with room for maxNbFS file systems (0 or less: no limit).
// 0x802F4310
int RtFSManagerOpen(int maxNbFS) {
    fsm.curNbFS = 0;
    fsm.maxNbFS = maxNbFS;
    fsm.CallBacks.errorCallBack = NULL;
    defaultFileSystem = NULL;
    fsm.fs.link.next = NULL;
    fsm.isOpened = 1;
    return 1;
}

// Tests whether one more file system fits; reports NOTREGISTRABLE when it does not.
// 0x802F4340
RwBool _rtFSManagerIsNewFSRegistrable(void) {
    if (fsm.maxNbFS > 0 && fsm.curNbFS + 1 > fsm.maxNbFS) {
        TkFSManagerTriggerErrorCallBack(RTFSM_ERROR_NOTREGISTRABLE);
        return FALSE;
    }
    return TRUE;
}

// Adds fs to the list unless the list is full or its name is taken.
// The name lookup reports NOFS to the error call-back when the name is free (the original's quirk).
// 0x802F43A4
int RtFSManagerRegister(RtFileSystem *fs) {
    if (_rtFSManagerIsNewFSRegistrable()) {
        if (RtFSManagerGetFileSystemFromName(fs->name) == NULL) {
            _rtFileSystemTriggerCallBack(fs, RTFS_CALLBACKCODE_FSREGISTER);
            fs->next = fsm.fs.link.next;
            fsm.fs.link.next = (RwSLLink *)fs;
            fsm.curNbFS++;
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

// Finds a registered file system by its name; reports NOFS to the error call-back when there is none.
// 0x802F44CC
RtFileSystem *RtFSManagerGetFileSystemFromName(char *fsName) {
    RtFileSystem *fs;

    for (fs = (RtFileSystem *)fsm.fs.link.next; fs != NULL; fs = (RtFileSystem *)fs->next) {
        if (rwstrcmp(fs->name, fsName) == 0) {
            return fs;
        }
    }
    TkFSManagerTriggerErrorCallBack(RTFSM_ERROR_NOFS);
    return NULL;
}

// Tests whether a file system already has this name; reports FSNAMEINUSE when one has.
// 0x802F4560
RwBool _rtFSManagerIsFSNameInUse(char *fsName) {
    if (RtFSManagerGetFileSystemFromName(fsName) != NULL) {
        TkFSManagerTriggerErrorCallBack(RTFSM_ERROR_FSNAMEINUSE);
        return TRUE;
    }
    return FALSE;
}

// Takes a closed file slot of fs and opens fileName in it. Returns NULL when the open fails.
// 0x802F4624
RtFile *_rtFSManagerFOpenOnFS(RtFileSystem *fs, char *fileName, RtFileAccessFlag access,
                              void (*CallBack)(void *, unsigned int, unsigned int, RtFSFileOperation, void *),
                              void *CallBackData) {
    RtFile *file = NULL;
    int index;

    if (fs != NULL) {
        OSDisableInterrupts();
        for (index = 0; index < fs->maxNbOpenFiles; index++) {
            RtFile *nextFile = fs->fsGetObject(fs, index);

            if (nextFile->status == RTFILE_STATUS_CLOSED) {
                nextFile->status = RTFILE_STATUS_OPENING;
                file = nextFile;
                break;
            }
        }
        OSEnableInterrupts();
        if (file == NULL) {
            fsm.lastError = RTFSM_ERROR_TOOMANYOPENFILES;
            TkFSManagerTriggerErrorCallBack(RTFSM_ERROR_TOOMANYOPENFILES);
            return NULL;
        } else {
            RtFileSystemError fsErr;

            if ((access & RTFILE_ACCESS_OPEN_ASYNC) || file->isAsync) {
                file->outstandingFileOp = RTFS_FILEOP_OPEN;
                file->outstandingCB = CallBack;
                file->outstandingCBData = CallBackData;
            }
            if (fs->defaultToAsync) {
                file->outstandingFileOp = RTFS_FILEOP_OPEN;
                access |= RTFILE_ACCESS_OPEN_ASYNC;
                file->outstandingCB = CallBack;
                file->outstandingCBData = CallBackData;
            }
            fsErr = fs->fsFileFunc.open(fs, file, fileName, access);
            if (fsErr != RTFS_ERROR_NOERROR) {
                file->status = RTFILE_STATUS_CLOSED;
                file = NULL;
            }
        }
        if (file != NULL) {
            file->priority = 2;
        }
        return file;
    }
    TkFSManagerTriggerErrorCallBack(RTFSM_ERROR_INVALIDFS);
    return NULL;
}

// Finds the file system that has the given device name (the "dev:" prefix of a path).
// 0x802F47A0
RtFileSystem *_rtGetFileSystemFromDeviceName(char *deviceName) {
    RtFileSystem *fs;

    for (fs = (RtFileSystem *)fsm.fs.link.next; fs != NULL; fs = (RtFileSystem *)fs->next) {
        if (rwstricmp(deviceName, fs->deviceName) == 0) {
            return fs;
        }
    }
    return NULL;
}

// Opens fileName on the file system its device prefix names, or on the default one.
// no copy in retail (dead-stripped or inlined)
RtFile *RtFSManagerFOpen(const char *filename, RtFileAccessFlag access,
                         void (*CallBack)(void *, unsigned int, unsigned int, RtFSFileOperation, void *),
                         void *CallBackData) {
    RtFileSystem *fs = RtFSManagerGetFileSystemFromFileName(filename);

    if (fs != NULL) {
        return _rtFSManagerFOpenOnFS(fs, (char *)filename, access, CallBack, CallBackData);
    }
    return NULL;
}

// Finds the file system a path names by its device prefix, else the default one; reports NOFS
// to the error call-back when there is none.
// The Debug build has it as a function; Final inlines it into RtFSManagerFOpen and RwFexist, which
// auto-inlining alone does not do for a body this size, so it is declared inline.
// no copy in retail (dead-stripped or inlined)
inline RtFileSystem *RtFSManagerGetFileSystemFromFileName(const char *fileName) {
    unsigned int nIndex;
    char deviceName[100];
    RtFileSystem *fs;

    if (fsm.curNbFS != 0) {
        unsigned int len = rwstrlen(fileName);

        for (nIndex = 0; nIndex < len; nIndex++) {
            if (fileName[nIndex] == ':') {
                strncpy(deviceName, fileName, nIndex + 1);
                deviceName[nIndex + 1] = '\0';
                fs = _rtGetFileSystemFromDeviceName(deviceName);
                if (fs != NULL) {
                    return fs;
                }
            }
        }
        fs = defaultFileSystem;
        if (fs != NULL) {
            return fs;
        }
    }
    TkFSManagerTriggerErrorCallBack(RTFSM_ERROR_NOFS);
    return NULL;
}

// Sets the file system paths without a device prefix go to.
// 0x802F4814
void RtFSManagerSetDefaultFileSystem(RtFileSystem *fs) {
    defaultFileSystem = fs;
}

// fopen: reads the mode letters ("r", "w", "a", "+", "b") into access flags and opens the file.
// 0x802F481C
void *RwFopen(const RwChar *name, const RwChar *access) {
    RtFile *file;
    RtFileAccessFlag flags = (RtFileAccessFlag)0;
    int plus = 0;
    int bin = 0;

    if (strstr(access, "+") != NULL) {
        plus = 1;
    }
    if (strstr(access, "b") != NULL) {
        bin = 1;
    }
    if (strstr(access, "r") != NULL) {
        flags |= RTFILE_ACCESS_OPEN_READ;
        if (plus) {
            flags |= RTFILE_ACCESS_OPEN_WRITE | RTFILE_ACCESS_OPEN_APPEND;
        }
    }
    if (strstr(access, "w") != NULL) {
        flags |= RTFILE_ACCESS_OPEN_WRITE | RTFILE_ACCESS_OPEN_CREATE;
        if (plus) {
            flags |= RTFILE_ACCESS_OPEN_READ;
        }
    }
    if (strstr(access, "a") != NULL) {
        flags |= RTFILE_ACCESS_OPEN_WRITE | RTFILE_ACCESS_OPEN_APPEND;
        if (plus) {
            flags |= RTFILE_ACCESS_OPEN_READ;
        }
        if (bin) {
            flags |= RTFILE_ACCESS_OPEN_CREATE;
        }
    }
    file = RtFSManagerFOpen(name, flags, NULL, NULL);
    return file;
}

// fclose: marks the file closed and has its file system close it. Always returns 0.
// 0x802F4A14
RwInt32 RwFclose(void *fptr) {
    RtFile *file = (RtFile *)fptr;
    RtFileSystemFileFunctionTable *fileFuncs = &file->fileSystem->fsFileFunc;

    OSDisableInterrupts();
    file->status = RTFILE_STATUS_CLOSED;
    fileFuncs->close(file);
    OSEnableInterrupts();
    return 0;
}

// fread: reads size * count bytes; returns the number of whole items.
// 0x802F4A6C
size_t RwFread(void *addr, size_t size, size_t count, void *fptr) {
    RtFile *file = (RtFile *)fptr;
    RtFileSystemFileFunctionTable *fileFuncs = &file->fileSystem->fsFileFunc;
    unsigned int bytesRead;

    bytesRead = fileFuncs->read(file, addr, size * count);
    return bytesRead / size;
}

// fwrite: writes size * count bytes; returns the number of whole items.
// 0x802F4AB4
size_t RwFwrite(const void *addr, size_t size, size_t count, void *fptr) {
    RtFile *file = (RtFile *)fptr;
    RtFileSystemFileFunctionTable *fileFuncs = &file->fileSystem->fsFileFunc;
    unsigned int bytesWritten;

    bytesWritten = fileFuncs->write(file, (void *)addr, size * count);
    return bytesWritten / size;
}

// fseek: sets the file position from the start, the current position or the end.
// Returns 0, or -1 for an unknown origin.
// 0x802F4AFC
RwInt32 RwFseek(void *fptr, long offset, RwInt32 origin) {
    RtFile *file = (RtFile *)fptr;
    RtFileSystemFileFunctionTable *fileFuncs = &file->fileSystem->fsFileFunc;

    if (file->isAsync) {
        file->outstandingFileOp = RTFS_FILEOP_SEEK;
        file->status = RTFILE_STATUS_BUSY;
    }
    switch (origin) {
    case SEEK_CUR:
        fileFuncs->setPosition(file, offset, RTFILE_POS_CURRENT);
        break;
    case SEEK_END:
        fileFuncs->setPosition(file, offset, RTFILE_POS_END);
        break;
    case SEEK_SET:
        fileFuncs->setPosition(file, offset, RTFILE_POS_BEGIN);
        break;
    default:
        return -1;
    }
    return 0;
}

// feof: asks the file system whether the file is at its end.
// 0x802F4B9C
RwInt32 RwFeof(void *fptr) {
    RtFile *file = (RtFile *)fptr;

    return file->fileSystem->fsFileFunc.isEOF(file);
}

// Tests whether the file system a path names can find the file.
// 0x802F4BAC
RwBool RwFexist(const RwChar *name) {
    RtFileSystem *fs = RtFSManagerGetFileSystemFromFileName(name);

    if (fs == NULL) {
        return FALSE;
    }
    return fs->fsFileFunc.fExists(fs, (char *)name);
}

// ftell: the low word of the file position.
// 0x802F4CE4
long RwFtell(void *fptr) {
    RtFile *file = (RtFile *)fptr;
    int pos;

    pos = (int)file->position.supportValue;
    return pos;
}
