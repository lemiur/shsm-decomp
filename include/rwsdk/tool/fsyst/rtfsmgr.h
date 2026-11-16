/*
 * rtfsmgr.h: the RtFileSystem manager's state record, its error codes and its entry points.
 * Names and layouts: Gormiti DWARF (RpFSyst.a rtfsmgr.obj).
 */
#ifndef RWSDK_TOOL_FSYST_RTFSMGR_H
#define RWSDK_TOOL_FSYST_RTFSMGR_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/tool/fsyst/rtfsyst.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwSLLink and RwSingleList: the singly linked list the manager keeps its file systems on. */
typedef struct RwSLLink RwSLLink;
struct RwSLLink {
    RwSLLink *next;                                  /* 0x00 */
};

typedef struct RwSingleList {
    RwSLLink link;                                   /* 0x00 */
} RwSingleList;

typedef enum RtFSManagerError {
    RTFSM_ERROR_NOERROR = 1,
    RTFSM_ERROR_INSTALL = 2,
    RTFSM_ERROR_UNINSTALL = 3,
    RTFSM_ERROR_INVALIDFS = 4,
    RTFSM_ERROR_NOTREGISTRABLE = 5,
    RTFSM_ERROR_NOFS = 6,
    RTFSM_ERROR_FSNAMEINUSE = 7,
    RTFSM_ERROR_TOOMANYOPENFILES = 8,
    RTFSM_ERROR_TYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RtFSManagerError;

typedef struct FSManagerCallBacks {
    void (*errorCallBack)(RtFSManagerError);         /* 0x00 */
} FSManagerCallBacks;

typedef struct RtFSManager {
    RwSingleList fs;                                 /* 0x00 */
    int maxNbFS;                                     /* 0x04 */
    int curNbFS;                                     /* 0x08 */
    int isOpened;                                    /* 0x0C */
    RtFSManagerError lastError;                      /* 0x10 */
    FSManagerCallBacks CallBacks;                    /* 0x14 */
} RtFSManager;

int RtFSManagerOpen(int maxNbFS);
int RtFSManagerRegister(RtFileSystem *fs);
RtFileSystem *RtFSManagerGetFileSystemFromName(char *fsName);
RtFile *RtFSManagerFOpen(const char *filename, RtFileAccessFlag access,
                         void (*CallBack)(void *, unsigned int, unsigned int, RtFSFileOperation, void *),
                         void *CallBackData);
RtFileSystem *RtFSManagerGetFileSystemFromFileName(const char *fileName);
void RtFSManagerSetDefaultFileSystem(RtFileSystem *fs);

#ifdef __cplusplus
}
#endif

#endif /* RWSDK_TOOL_FSYST_RTFSMGR_H */
