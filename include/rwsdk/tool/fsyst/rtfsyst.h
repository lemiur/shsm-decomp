/*
 * rtfsyst.h: the RtFileSystem toolkit's file-system and file records, and the file-system
 * manager entry points rtfsyst.c calls. Layouts come from the Gormiti DWARF (sibling.exe types).
 */
#ifndef RWSDK_TOOL_FSYST_RTFSYST_H
#define RWSDK_TOOL_FSYST_RTFSYST_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/badevice.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RtFileSystemError {
    RTFS_ERROR_NOERROR = 1,
    RTFS_ERROR_TOOMANYFILESYSTEMS = 2,
    RTFS_ERROR_FILENOTFOUND = 3,
    RTFS_ERROR_WRITEPROTECTED = 4,
    RTFS_ERROR_FILENAMETOOLONG = 5,
    RTFS_ERROR_FILESYSTEMNAMEINUSE = 6,
    RTFS_ERROR_FILESYSTEMDEVICENAMEREGISTERED = 7
} RtFileSystemError;

typedef enum RtFileSystemStatus {
    RTFS_STATUS_NOTREADY = 1,
    RTFS_STATUS_READY = 2,
    RTFS_STATUS_DOOROPEN = 3,
    RTFS_STATUS_NOMEDIA = 4,
    RTFS_STATUS_WRONGMEDIA = 5,
    RTFS_STATUS_RETRYING = 6,
    RTFS_STATUS_FATAL = 7
} RtFileSystemStatus;

typedef enum RtFSFileOperation {
    RTFS_FILEOP_NONE = 1,
    RTFS_FILEOP_OPEN = 2,
    RTFS_FILEOP_CLOSE = 3,
    RTFS_FILEOP_SEEK = 4,
    RTFS_FILEOP_READ = 5
} RtFSFileOperation;

typedef enum RtFileAccessFlag {
    RTFILE_ACCESS_OPEN_READ = 1,
    RTFILE_ACCESS_OPEN_WRITE = 2,
    RTFILE_ACCESS_OPEN_CREATE = 4,
    RTFILE_ACCESS_OPEN_APPEND = 8,
    RTFILE_ACCESS_OPEN_ASYNC = 16,
    RTFILE_ACCESS_NO_BUFFERING = 32
} RtFileAccessFlag;

typedef enum RtFileStatus {
    RTFILE_STATUS_CLOSED = 1,
    RTFILE_STATUS_READY = 2,
    RTFILE_STATUS_BUSY = 3,
    RTFILE_STATUS_ERROR = 4,
    RTFILE_STATUS_OPENING = 5
} RtFileStatus;

typedef enum RtFileError {
    RTFILE_ERROR_NOERROR = 1,
    RTFILE_ERROR_FILESYSTEM = 2,
    RTFILE_ERROR_FILE = 3,
    RTFILE_ERROR_DISKFULL = 4
} RtFileError;

typedef enum RtFileSeekMethod {
    RTFILE_POS_BEGIN = 1,
    RTFILE_POS_CURRENT = 2,
    RTFILE_POS_END = 3
} RtFileSeekMethod;

/* The call-back codes _rtFileSystemTriggerCallBack takes (names and values: Gormiti DWARF). */
typedef enum RtFileSystemCallBackCode {
    RTFS_CALLBACKCODE_FSREGISTER = 1,
    RTFS_CALLBACKCODE_FSUNREGISTER = 2,
    RTFS_CALLBACKCODE_TYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RtFileSystemCallBackCode;

/* RtInt64: a 64-bit file length or offset. The support value is the long long; the other view is two words. */
typedef struct _rtInt64 {
    int low;
    int high;
} _rtInt64;

typedef union RtInt64 {
    long long supportValue;
    _rtInt64 noSupportValue;
} RtInt64;

typedef struct RtFile RtFile;
typedef struct RtFileSystem RtFileSystem;

typedef struct rtFileSystemCallBacks {
    void (*registerCallBack)(char *);                /* 0x00 */
    void (*unregisterCallBack)(char *);              /* 0x04 */
} rtFileSystemCallBacks;

typedef struct RtFileSystemFileFunctionTable {
    RtFileSystemError (*open)(RtFileSystem *, RtFile *, char *, unsigned int);  /* 0x00 */
    void (*close)(RtFile *);                         /* 0x04 */
    unsigned int (*read)(RtFile *, void *, unsigned int);  /* 0x08 */
    unsigned int (*write)(RtFile *, void *, unsigned int); /* 0x0C */
    RtInt64 (*setPosition)(RtFile *, int, RtFileSeekMethod); /* 0x10 */
    RtFileStatus (*sync)(RtFile *, int);             /* 0x14 */
    int (*abort)(RtFile *);                          /* 0x18 */
    int (*isEOF)(RtFile *);                          /* 0x1C */
    RtFileStatus (*getStatus)(RtFile *);             /* 0x20 */
    int (*fExists)(RtFileSystem *, char *);          /* 0x24 */
} RtFileSystemFileFunctionTable;

struct RtFileSystem {
    struct RwSLLink *next;                           /* 0x00 */
    int maxNbOpenFiles;                              /* 0x04 */
    RtFileSystemStatus status;                       /* 0x08 */
    char *deviceName;                                /* 0x0C */
    int defaultToAsync;                              /* 0x10 */
    RtFile *(*fsGetObject)(RtFileSystem *, unsigned int);  /* 0x14 */
    void (*fsClose)(RtFileSystem *);                 /* 0x18 */
    char *(*fsGetDeviceName)(RtFileSystem *);        /* 0x1C */
    rtFileSystemCallBacks CallBacks;                 /* 0x20 */
    RtFileSystemFileFunctionTable fsFileFunc;        /* 0x28 */
    char name[5];                                    /* 0x50 */
};

struct RtFile {
    RtInt64 length;                                  /* 0x00 */
    RtInt64 position;                                /* 0x08 */
    RtInt64 initialPos;                              /* 0x10 */
    int isAsync;                                     /* 0x18 */
    int priority;                                    /* 0x1C */
    RtFileStatus status;                             /* 0x20 */
    RtFileError error;                               /* 0x24 */
    RtFSFileOperation outstandingFileOp;             /* 0x28 */
    unsigned int outstandingSize;                    /* 0x2C */
    void (*outstandingCB)(void *, unsigned int, unsigned int, RtFSFileOperation, void *); /* 0x30 */
    void *outstandingCBData;                         /* 0x34 */
    RtFileSystem *fileSystem;                        /* 0x38 */
    char *fileName;                                  /* 0x3C */
};

/* File-system manager entry points (defined in the manager unit). */
RwBool _rtFSManagerIsNewFSRegistrable(void);
RwBool _rtFSManagerIsFSNameInUse(char *fsName);
RtFileSystem *_rtGetFileSystemFromDeviceName(char *deviceName);
RtFile *_rtFSManagerFOpenOnFS(RtFileSystem *fs, char *fileName, RtFileAccessFlag access,
                              void (*CallBack)(void *, unsigned int, unsigned int, RtFSFileOperation, void *),
                              void *CallBackData);

/* Functions of rtfsyst.c. */
RtFileSystemError RtFileSystemSetDeviceName(RtFileSystem *fs, char *deviceName);

RtFileSystemError _rtFSInit(RtFileSystem *fs, int maxNbOpenFiles, char *fsName, char *deviceName);
char *_rtGetFileNameFromDeviceName(char *path);
char *_rtFSBuildFileName(char *buffer, int bufferLength, char *driveName, char *filePath, char cSeparator);
int _rtFSFileExists(RtFileSystem *fs, char *fileName);
int _rtFSIsEOF(RtFile *file);
void _rtFileSystemTriggerCallBack(RtFileSystem *fs, RtFileSystemCallBackCode cbCode);
void rtFSTriggerFileOutstandingOpCallback(RtFile *file);

#ifdef __cplusplus
}
#endif

#endif /* RWSDK_TOOL_FSYST_RTFSYST_H */
