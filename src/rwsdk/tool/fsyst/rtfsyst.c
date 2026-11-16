// Unit name and boundary: Gormiti link map (RpFSyst.a rtfsyst.obj)
/*
 * rtfsyst.c: the RtFileSystem manager's file-system registry and file records: file-system
 * initialisation, file-name helpers, the EOF test and the call-back triggers.
 */
#include <string.h>
#include <rwsdk/tool/fsyst/rtfsyst.h>

// Registers a file system: its name, its file slots and its device name. Returns the registration result.
// 0x802F4CEC
RtFileSystemError _rtFSInit(RtFileSystem *fs, int maxNbOpenFiles, char *fsName, char *deviceName) {
    int i;

    if (_rtFSManagerIsNewFSRegistrable()) {
        if (_rtFSManagerIsFSNameInUse(fsName)) {
            return RTFS_ERROR_FILESYSTEMNAMEINUSE;
        }
        rwstrcpy(fs->name, fsName);
        fs->maxNbOpenFiles = maxNbOpenFiles;
        fs->defaultToAsync = 0;
        for (i = 0; i < maxNbOpenFiles; i++) {
            RtFile *file = fs->fsGetObject(fs, i);

            file->status = RTFILE_STATUS_CLOSED;
            file->isAsync = fs->defaultToAsync;
        }
    } else {
        return RTFS_ERROR_TOOMANYFILESYSTEMS;
    }
    fs->deviceName = NULL;
    {
        RtFileSystemError ret = RtFileSystemSetDeviceName(fs, deviceName);

        if (ret != RTFS_ERROR_NOERROR) {
            return ret;
        }
    }
    fs->status = RTFS_STATUS_READY;
    fs->CallBacks.registerCallBack = NULL;
    fs->CallBacks.unregisterCallBack = NULL;
    return RTFS_ERROR_NOERROR;
}

// Returns the part of a path after its first colon, or the whole path when it has no colon.
// Local names and declaration order (nIndex, fPath) come from the Gormiti DWARF.
// 0x802F4E5C
char *_rtGetFileNameFromDeviceName(char *fileName) {
    int nIndex;
    char *fPath = fileName;

    for (nIndex = 0; nIndex < (int)rwstrlen(fileName); nIndex++) {
        if (fPath[nIndex] == ':') {
            fileName = &fPath[nIndex + 1];
            break;
        }
    }
    return fileName;
}

// Builds driveName + filePath in buffer, turning slashes in filePath into cSeparator. Returns NULL if it does not fit.
// 0x802F4EE0
char *_rtFSBuildFileName(char *buffer, int bufferLength, char *driveName, char *filePath, char cSeparator) {
    int driveLength;
    int pathLength;
    int index;

    driveLength = strlen(driveName);
    pathLength = strlen(filePath);
    if (driveLength + pathLength > bufferLength - 1) {
        return NULL;
    }
    rwstrcpy(buffer, driveName);
    for (index = 0; index <= pathLength; index++) {
        if (filePath[index] == '/' || filePath[index] == '\\') {
            buffer[driveLength + index] = cSeparator;
        } else {
            buffer[driveLength + index] = filePath[index];
        }
    }
    return buffer;
}

// Tests whether fileName opens on fs; an opened file is closed again.
// 0x802F4FA8
int _rtFSFileExists(RtFileSystem *fs, char *fileName) {
    RtFile *file;

    file = _rtFSManagerFOpenOnFS(fs, fileName, RTFILE_ACCESS_OPEN_READ, NULL, NULL);
    if (file != NULL) {
        fs->fsFileFunc.close(file);
        return 1;
    }
    return 0;
}

// Tests for end of file: a position of -1 or a position at or past the length.
// 0x802F5000
int _rtFSIsEOF(RtFile *file) {
    long long position;

    position = file->position.supportValue;
    if (position == -1) {
        return 1;
    }
    return position >= file->length.supportValue;
}

// Calls the register or unregister call-back of fs with its name.
// 0x802F504C
void _rtFileSystemTriggerCallBack(RtFileSystem *fs, RtFileSystemCallBackCode cbCode) {
    switch (cbCode) {
    case RTFS_CALLBACKCODE_FSREGISTER:
        if (fs->CallBacks.registerCallBack != NULL) {
            fs->CallBacks.registerCallBack(fs->name);
        }
        break;
    case RTFS_CALLBACKCODE_FSUNREGISTER:
        if (fs->CallBacks.unregisterCallBack != NULL) {
            fs->CallBacks.unregisterCallBack(fs->name);
        }
        break;
    }
}

// Gives fs its device name unless another file system already has it. Retail inlines it into _rtFSInit.
// no copy in retail (dead-stripped)
RtFileSystemError RtFileSystemSetDeviceName(RtFileSystem *fs, char *deviceName) {
    if (_rtGetFileSystemFromDeviceName(deviceName) == NULL) {
        if (fs->deviceName != NULL) {
            RwFree(fs->deviceName);
        }
        // 0x1BE: the allocation's module id (name unconfirmed)
        fs->deviceName = RwMalloc(rwstrlen(deviceName) + 1, rwMEMHINTDUR_GLOBAL | 0x1BE);
        rwstrcpy(fs->deviceName, deviceName);
    } else {
        return RTFS_ERROR_FILESYSTEMDEVICENAMEREGISTERED;
    }
    return RTFS_ERROR_NOERROR;
}

// Reports the outstanding asynchronous operation of an asynchronous file to its call-back, then clears it.
// 0x802F5094
void rtFSTriggerFileOutstandingOpCallback(RtFile *file) {
    if (file != NULL && file->isAsync) {
        if (file->outstandingFileOp != RTFS_FILEOP_NONE && file->outstandingCB != NULL) {
            RtFSFileOperation fileOp;
            int data;

            fileOp = file->outstandingFileOp;
            if (fileOp == RTFS_FILEOP_SEEK) {
                data = (int)file->position.supportValue;
            } else {
                data = file->outstandingSize;
            }
            file->outstandingFileOp = RTFS_FILEOP_NONE;
            file->outstandingCB(file, data, file->status, fileOp, file->outstandingCBData);
        } else {
            file->outstandingFileOp = RTFS_FILEOP_NONE;
        }
    }
}
