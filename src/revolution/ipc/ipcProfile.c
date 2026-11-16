// Reference: zeldaret/tp libs/revolution/src/ipc/ipcProfile.c
#include <revolution/ipc.h>

#define IPC_REQ_MAX 0x20

static s32 IpcFdArray[IPC_REQ_MAX] = {0};
static u32 IpcReqPtrArray[IPC_REQ_MAX] = {0};

static u32 IpcNumPendingReqs = 0;
static u32 IpcNumUnIssuedReqs = 0;

static void AddReqInfo(void* req, s32 fd);
static void DelReqInfo(void* req, s32 fd);

/*
 * IPCiProfInit
 *
 * Clears the request-profiling tables.
 */
void IPCiProfInit(void) {
    u32 i;

    IpcNumPendingReqs = 0;
    IpcNumUnIssuedReqs = 0;

    for (i = 0; i < IPC_REQ_MAX; ++i) {
        IpcReqPtrArray[i] = 0;
        IpcFdArray[i] = -1;
    }
} // 0x80243AA0

/*
 * IPCiProfQueueReq
 *
 * Records a request queued to IOS.
 */
void IPCiProfQueueReq(void* req, s32 handle) {
    ++IpcNumPendingReqs;
    ++IpcNumUnIssuedReqs;
    AddReqInfo(req, handle);
} // 0x80243B60

/*
 * IPCiProfAck
 *
 * Counts a queued request as issued once IOS acknowledges it.
 */
void IPCiProfAck(void) {
    --IpcNumUnIssuedReqs;
} // 0x80243BF0

/*
 * IPCiProfReply
 *
 * Removes a replied request from the profiling tables.
 */
void IPCiProfReply(void* req, s32 handle) {
    --IpcNumPendingReqs;
    DelReqInfo(req, handle);
} // 0x80243C00

/*
 * AddReqInfo
 *
 * Stores a request in the first free profiling slot (inlined into IPCiProfQueueReq).
 */
static void AddReqInfo(void* ptr, s32 fd) {
    u32 i;

    for (i = 0; i < IPC_REQ_MAX; ++i) {
        if (IpcReqPtrArray[i] == 0 && IpcFdArray[i] == -1) {
            IpcReqPtrArray[i] = (u32)ptr;
            IpcFdArray[i] = fd;
            break;
        }
    }
}

/*
 * DelReqInfo
 *
 * Clears the profiling slot of a request (inlined into IPCiProfReply).
 */
static void DelReqInfo(void* ptr, s32 fd) {
    u32 i;

    for (i = 0; i < IPC_REQ_MAX; ++i) {
        if (IpcReqPtrArray[i] == (u32)ptr && IpcFdArray[i] == fd) {
            IpcReqPtrArray[i] = 0;
            IpcFdArray[i] = -1;
            break;
        }
    }
}
