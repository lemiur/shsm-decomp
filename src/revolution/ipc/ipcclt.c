// Reference: zeldaret/tp libs/revolution/src/ipc/ipcclt.c
//
// IOS resource manager client. This build is the SDK revision with 16 queued responses, 0x20 request
// buffers and the IOS_IoctlvReboot relaunch bookkeeping (__relnchRpcSave); the small static helpers
// (ipcAllocReq, ipcFree, __ipcQueueRequest, __ipcSendRequest, IpcAckHandler, __ios_Ipc1, __ios_Open ...)
// are inlined into their callers and have no retail address of their own.
#include <revolution/ipc.h>
#include <revolution/hardware.h>
#include <revolution/os.h>

#include <string.h>

#define RESPONSE_REQ_BUF_LEN 0x10
#define IPC_BUF_CNT 0x20

/* IPC register indices (IPCReadReg/IPCWriteReg). */
#define IPC_REG_ADDR 0
#define IPC_REG_CTRL 1
#define IPC_REG_REPLY 2

/* IPC control register: Y1 sends a request, X1 a reply is pending, Y2 acknowledge pending, X2 reply handled;
   bits 4 and 5 enable the reply and acknowledge interrupts. */
#define IPC_Y1 (1 << 0)
#define IPC_Y2 (1 << 1)
#define IPC_X1 (1 << 2)
#define IPC_X2 (1 << 3)
#define IPC_REPLY_INT_ENABLE (1 << 4)
#define IPC_ACK_INT_ENABLE (1 << 5)
#define IPC_INT_ENABLE (IPC_ACK_INT_ENABLE | IPC_REPLY_INT_ENABLE)

/* Hollywood interrupt flag (word 0x30) and its IPC bit, cleared by writing the bit back. */
#define ACR_PPC_INTSTS (0x30 / 4)
#define ACR_INTSTS_IPC 0x40000000

/* Entries between the read and write counters, counted modulo 2^32. */
#define diff(a, b) ((a) < (b)) ? ((u32)0xffffffff - (b) + (a) + 1) : ((a) - (b))

#define OSVirtualToPhysical(addr) OSCachedToPhysical(addr)
#define OSPhysicalToVirtual(addr) OSPhysicalToCached(addr)

static s32 __mailboxAck = 1;
static u32 __relnchFl = 0;

typedef struct IOSRpcRequest {
    IOSResourceRequest request;
    IOSIpcCb cb ATTRIBUTE_ALIGN(32);
    void* callback_arg;
    u32 relaunch_flag;
    OSThreadQueue thread_queue;
} IOSRpcRequest;

static IOSRpcRequest* __relnchRpc = 0;
static IOSRpcRequest* __relnchRpcSave = 0;

#define ROUNDUP(sz) (((u32)(sz) + (IPC_BUF_CNT / 2 - 1)) & ~(u32)(IPC_BUF_CNT / 2 - 1))

static u8 __rpcBuf[ROUNDUP(sizeof(IOSRpcRequest))] ATTRIBUTE_ALIGN(32);

static struct {
    u32 rcount;
    u32 wcount;
    u32 rptr;
    u32 wptr;
    IOSResourceRequest* buf[RESPONSE_REQ_BUF_LEN];
} __responses;

static OSAlarm __timeout_alarm;

static IOSHeapId hid = -1;

/*
 * strnlen
 *
 * Length of str, counting at most n bytes; defined here so the IOS_Open callers can inline it.
 */
u32 strnlen(const u8* str, u32 n) {
    const u8* s = str;
    while (*s && n-- > 0) {
        ++s;
    }
    return (s - str);
} // 0x80241C70

/*
 * ipcAllocReq
 *
 * Allocates one 32-byte aligned request block from the IPC heap.
 */
static IOSRpcRequest* ipcAllocReq(void) {
    IOSRpcRequest* req = NULL;
    req = iosAllocAligned(hid, 0x40, 0x20);
    return req;
}

/*
 * ipcFree
 *
 * Returns a request block to the IPC heap.
 */
static IOSError ipcFree(IOSRpcRequest* rpc) {
    IOSError ret = 0;
    iosFree(hid, rpc);
    return ret;
}

/*
 * __ipcQueueRequest
 *
 * Appends req to the ring of requests waiting to be sent; IOS_ERROR_QFULL when the ring is full.
 */
static IOSError __ipcQueueRequest(IOSResourceRequest* req) {
    IOSError ret = 0;

    if (diff(__responses.wcount, __responses.rcount) >= sizeof(__responses.buf) / sizeof(__responses.buf[0])) {
        ret = -8;
    } else {
        __responses.buf[__responses.wptr] = req;
        __responses.wptr = (__responses.wptr + 1) % (sizeof(__responses.buf) / sizeof(__responses.buf[0]));
        __responses.wcount++;
        IPCiProfQueueReq(req, (s32)req->handle);
    }

    return ret;
}

/*
 * __ipcSendRequest
 *
 * Posts the oldest queued request to IOS and raises Y1; a relaunch request also takes the second mailbox credit.
 */
static void __ipcSendRequest(void) {
    IOSRpcRequest* rpc;

    if (diff(__responses.wcount, __responses.rcount) <= 0) {
        return;
    }

    rpc = (IOSRpcRequest*)__responses.buf[__responses.rptr];

    if (rpc == 0) {
        return;
    }

    if (rpc->relaunch_flag) {
        __mailboxAck--;
    }

    IPCWriteReg(IPC_REG_ADDR, OSVirtualToPhysical(rpc));
    __responses.rptr = (__responses.rptr + 1) % (sizeof(__responses.buf) / sizeof(__responses.buf[0]));
    __responses.rcount++;
    __mailboxAck--;

    IPCWriteReg(IPC_REG_CTRL, (IPCReadReg(IPC_REG_CTRL) & IPC_INT_ENABLE) | IPC_Y1);
}

/*
 * IpcReplyHandler
 *
 * Takes the reply IOS posted: converts the request's pointers back to cached addresses, invalidates the
 * buffers IOS wrote, then runs the caller's callback or wakes the sleeping thread.
 */
void IpcReplyHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    IOSResourceRequest* req;
    IOSRpcRequest* rep;
    u32 addr;

    addr = IPCReadReg(IPC_REG_REPLY);

    if (!addr) {
        goto err;
    }

    rep = (IOSRpcRequest*)OSPhysicalToVirtual(addr);
    IPCWriteReg(IPC_REG_CTRL, (IPCReadReg(IPC_REG_CTRL) & IPC_INT_ENABLE) | IPC_X1);
    __ACRRestrictedRegs[ACR_PPC_INTSTS] = ACR_INTSTS_IPC;
    req = &rep->request;

    DCInvalidateRange(req, sizeof(*req));

    switch (req->handle) {
    case 3:
        req->args.read.outPtr = (req->args.read.outPtr) ? OSPhysicalToVirtual((u32)req->args.read.outPtr) : 0;

        if (req->status > 0) {
            DCInvalidateRange(req->args.read.outPtr, (u32)req->status);
        }

        break;
    case 6:
        req->args.ioctl.outPtr = (req->args.ioctl.outPtr) ? OSPhysicalToVirtual((u32)req->args.ioctl.outPtr) : 0;
        DCInvalidateRange(req->args.ioctl.inPtr, req->args.ioctl.inLen);
        DCInvalidateRange(req->args.ioctl.outPtr, req->args.ioctl.outLen);
        break;
    case 7: {
        int i;
        IOSResourceIoctlv* v = &req->args.ioctlv;
        req->args.ioctlv.vector =
            (req->args.ioctlv.vector) ? (IOSIoVector*)OSPhysicalToVirtual((u32)req->args.ioctlv.vector) : 0;
        DCInvalidateRange(&v->vector[0], (req->args.ioctlv.readCount + req->args.ioctlv.writeCount) * sizeof(IOSIoVector));

        for (i = 0; i < (req->args.ioctlv.readCount + req->args.ioctlv.writeCount); ++i) {
            v->vector[i].base = (v->vector[i].base) ? (u8*)OSPhysicalToVirtual((u32)v->vector[i].base) : 0;
            DCInvalidateRange(v->vector[i].base, v->vector[i].length);
        }

        if (__relnchFl && __relnchRpcSave == rep) {
            __relnchFl = 0;

            if (__mailboxAck < 1) {
                __mailboxAck++;
            }
        }

        break;
    }
    default:
        break;
    }

    if (rep->cb) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        rep->cb(req->status, (void*)rep->callback_arg);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
        ipcFree(rep);
    } else {
        OSWakeupThread(&rep->thread_queue);
    }

    IPCWriteReg(IPC_REG_CTRL, (IPCReadReg(IPC_REG_CTRL) & IPC_INT_ENABLE) | IPC_X2);
    IPCiProfReply(req, (s32)req->handle);

err:
    return;
} // 0x80241CA0

/*
 * IpcAckHandler
 *
 * Handles IOS accepting a request: returns the mailbox credit, wakes a pending relaunch and sends the next request.
 */
static void IpcAckHandler(__OSInterrupt interrupt, OSContext* context) {
    IPCWriteReg(IPC_REG_CTRL, (IPCReadReg(IPC_REG_CTRL) & IPC_INT_ENABLE) | IPC_Y2);
    __ACRRestrictedRegs[ACR_PPC_INTSTS] = ACR_INTSTS_IPC;

    if (__mailboxAck < 1) {
        __mailboxAck++;
        IPCiProfAck();
    }

    if (__mailboxAck > 0) {
        if (__relnchFl) {
            IOSResourceRequest* req = &__relnchRpc->request;
            req->status = 0;
            __relnchFl = 0;

            OSWakeupThread(&__relnchRpc->thread_queue);
            IPCWriteReg(IPC_REG_CTRL, (IPCReadReg(IPC_REG_CTRL) & IPC_INT_ENABLE) | IPC_X2);
        }

        __ipcSendRequest();
    }
}

/*
 * IPCInterruptHandler
 *
 * Dispatches the IPC interrupt: a reply (X1 pending, reply interrupt enabled) and/or an acknowledge (Y2
 * pending, acknowledge interrupt enabled).
 */
void IPCInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    if ((IPCReadReg(IPC_REG_CTRL) & (IPC_REPLY_INT_ENABLE | IPC_X1)) == (IPC_REPLY_INT_ENABLE | IPC_X1)) {
        IpcReplyHandler(interrupt, context);
    }

    if ((IPCReadReg(IPC_REG_CTRL) & (IPC_ACK_INT_ENABLE | IPC_Y2)) == (IPC_ACK_INT_ENABLE | IPC_Y2)) {
        IpcAckHandler(interrupt, context);
    }
} // 0x80241F00

/*
 * IPCCltInit
 *
 * First call only: creates the request heap in the IPC buffer and hooks the IPC interrupt.
 */
IOSError IPCCltInit(void) {
    static u32 initialized = 0;
    u32 i;
    IOSError ret = 0;
    void* bufferLo;

    if (initialized) {
        goto out;
    }

    initialized = 1;

    IPCInit();

    i = ROUNDUP(IPC_BUF_CNT * (ROUNDUP(sizeof(IOSRpcRequest)) + 64));
    bufferLo = IPCGetBufferLo();

    if ((void*)((u8*)bufferLo + i) > IPCGetBufferHi()) {
        ret = -22;
        goto out;
    }

    hid = iosCreateHeap(bufferLo, i);
    IPCSetBufferLo((void*)((u8*)bufferLo + i));

    __OSSetInterruptHandler(27, IPCInterruptHandler);
    __OSUnmaskInterrupts(16);

    IPCWriteReg(IPC_REG_CTRL, (IPC_INT_ENABLE | IPC_X2));
    IPCiProfInit();
    OSCreateAlarm(&__timeout_alarm);

out:
    return ret;
} // 0x802420B0

/*
 * IPCCltReInit
 *
 * Creates a new, smaller request heap from what is left of the IPC buffer (after an IOS reload).
 */
IOSError IPCCltReInit(void) {
    u32 i;
    IOSError ret = 0;
    void* bufferLo;

    i = ROUNDUP(IPC_BUF_CNT * ROUNDUP(sizeof(IOSRpcRequest)));
    bufferLo = IPCGetBufferLo();

    if ((void*)((u8*)bufferLo + i) > IPCGetBufferHi()) {
        ret = -22;
        goto out;
    }

    hid = iosCreateHeap(bufferLo, i);
    IPCSetBufferLo((void*)((u8*)bufferLo + i));

out:
    return ret;
} // 0x80242170

/*
 * __ios_Ipc1
 *
 * First half of every call: allocates the request and fills in command, handle and callback.
 */
static IOSError __ios_Ipc1(IOSFd fd, u32 cmd, IOSIpcCb cb, void* cbArg, IOSRpcRequest** rpc) {
    IOSError ret = 0;
    IOSResourceRequest* req;

    if (rpc == 0) {
        ret = -4;
        goto error;
    }

    *rpc = (IOSRpcRequest*)ipcAllocReq();

    if (*rpc == 0) {
        ret = -22;
        goto error;
    }

    req = &(*rpc)->request;
    (*rpc)->cb = cb;
    (*rpc)->callback_arg = cbArg;
    (*rpc)->relaunch_flag = 0;
    req->cmd = cmd;
    req->handle = (u32)fd;

error:
    return ret;
}

/*
 * __ios_Ipc2
 *
 * Second half: queues the request and sends it; a synchronous call sleeps until the reply and returns its status.
 */
static IOSError __ios_Ipc2(IOSRpcRequest* rpc, IOSIpcCb cb) {
    IOSError ret = 0;
    u32 inten;
    IOSResourceRequest* req;

    if (rpc == 0) {
        ret = -4;
    } else {
        req = &rpc->request;

        if (!cb) {
            OSInitThreadQueue(&rpc->thread_queue);
        }

        DCFlushRange(req, sizeof(*req));
        inten = OSDisableInterrupts();
        ret = __ipcQueueRequest(req);

        if (ret != 0) {
            OSRestoreInterrupts(inten);

            if (cb) {
                ipcFree(rpc);
            }
        } else {
            if (__mailboxAck > 0) {
                __ipcSendRequest();
            }

            if (!cb) {
                OSSleepThread(&rpc->thread_queue);
            }

            OSRestoreInterrupts(inten);

            if (!cb) {
                ret = req->status;
            }
        }
    }

    if (rpc && !cb) {
        ipcFree(rpc);
    }

    return ret;
} // 0x802421F0

/*
 * __ios_Open
 *
 * Fills in the path argument of an open request (the path is flushed to memory, its length bounded by 64).
 */
static IOSError __ios_Open(IOSRpcRequest* rpc, const char* path, u32 flags) {
    IOSError ret = 0;
    IOSResourceRequest* req;

    if (!rpc) {
        ret = -4;
        goto error;
    }

    req = &rpc->request;
    DCFlushRange((void*)path, strnlen((const u8*)path, 64) + 1);
    req->args.open.path = (u8*)OSVirtualToPhysical((void*)path);
    req->args.open.flags = flags;

error:
    return ret;
}

/*
 * IOS_OpenAsync
 *
 * Opens the resource at path; cb runs with the new descriptor when IOS replies.
 */
IOSError IOS_OpenAsync(const char* path, u32 flags, IOSIpcCb cb, void* cbArg) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(0, 1, cb, cbArg, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Open(rpc, path, flags);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, cb);
error:
    return ret;
} // 0x80242430

/*
 * IOS_Open
 *
 * Opens the resource at path and returns its descriptor.
 */
IOSError IOS_Open(const char* path, u32 flags) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(0, 1, 0, 0, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Open(rpc, path, flags);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, 0);

error:
    return ret;
} // 0x80242550

/*
 * IOS_CloseAsync
 *
 * Closes fd; cb runs when IOS replies.
 */
IOSError IOS_CloseAsync(IOSFd fd, IOSIpcCb cb, void* cbArg) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 2, cb, cbArg, &rpc);

    if (ret == 0) {
        ret = __ios_Ipc2(rpc, cb);
    }

    return ret;
} // 0x80242680

/*
 * IOS_Close
 *
 * Closes fd.
 */
IOSError IOS_Close(IOSFd fd) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 2, 0, 0, &rpc);

    if (ret == 0) {
        ret = __ios_Ipc2(rpc, 0);
    }

    return ret;
} // 0x80242740

/*
 * __ios_Read
 *
 * Fills in the output buffer of a read request.
 */
static IOSError __ios_Read(IOSRpcRequest* rpc, void* buf, u32 len) {
    IOSError ret = 0;
    IOSResourceRequest* req;

    if (!rpc) {
        ret = -4;
        goto error;
    }

    req = &rpc->request;

    DCInvalidateRange(buf, len);
    req->args.read.outPtr = (buf) ? (u8*)OSVirtualToPhysical(buf) : 0;
    req->args.read.outLen = len;

error:
    return ret;
}

/*
 * IOS_ReadAsync
 *
 * Reads up to len bytes from fd into buf; cb runs with the byte count when IOS replies.
 */
IOSError IOS_ReadAsync(IOSFd fd, void* buf, u32 len, IOSIpcCb cb, void* cbArg) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 3, cb, cbArg, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Read(rpc, buf, len);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, cb);

error:
    return ret;
} // 0x802427F0

/*
 * IOS_Read
 *
 * Reads up to len bytes from fd into buf and returns the byte count.
 */
IOSError IOS_Read(IOSFd fd, void* buf, u32 len) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 3, 0, 0, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Read(rpc, buf, len);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, 0);

error:
    return ret;
} // 0x802428F0

/*
 * __ios_Write
 *
 * Fills in the input buffer of a write request.
 */
static IOSError __ios_Write(IOSRpcRequest* rpc, void* buf, u32 len) {
    IOSError ret = 0;
    IOSResourceRequest* req;

    if (!rpc) {
        ret = -4;
        goto error;
    }

    req = &rpc->request;
    req->args.write.inPtr = (buf) ? (u8*)OSVirtualToPhysical(buf) : 0;
    req->args.write.inLen = len;
    DCFlushRange(buf, len);

error:
    return ret;
}

/*
 * IOS_WriteAsync
 *
 * Writes len bytes from buf to fd; cb runs when IOS replies.
 */
IOSError IOS_WriteAsync(IOSFd fd, void* buf, u32 len, IOSIpcCb cb, void* cbArg) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 4, cb, cbArg, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Write(rpc, buf, len);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, cb);

error:
    return ret;
} // 0x80242A00

/*
 * IOS_Write
 *
 * Writes len bytes from buf to fd.
 */
IOSError IOS_Write(IOSFd fd, void* buf, u32 len) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 4, 0, 0, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Write(rpc, buf, len);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, 0);

error:
    return ret;
} // 0x80242B00

/*
 * __ios_Seek
 *
 * Fills in the offset and whence of a seek request.
 */
static IOSError __ios_Seek(IOSRpcRequest* rpc, s32 offset, u32 whence) {
    IOSError ret = 0;
    IOSResourceRequest* req;

    if (!rpc) {
        ret = -4;
        goto error;
    }

    req = &rpc->request;
    req->args.seek.offset = offset;
    req->args.seek.whence = whence;

error:
    return ret;
}

/*
 * IOS_SeekAsync
 *
 * Moves the position of fd; cb runs with the new position when IOS replies.
 */
IOSError IOS_SeekAsync(IOSFd fd, s32 offset, u32 whence, IOSIpcCb cb, void* cbArg) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 5, cb, cbArg, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Seek(rpc, offset, whence);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, cb);

error:
    return ret;
} // 0x80242C10

/*
 * __ios_Ioctl
 *
 * Fills in the command and the input and output buffers of an ioctl request.
 */
static IOSError __ios_Ioctl(IOSRpcRequest* rpc, s32 cmd, void* input, u32 inputLen, void* output, u32 outputLen) {
    IOSError ret = 0;
    IOSResourceRequest* req;

    if (!rpc) {
        ret = -4;
        goto error;
    }

    req = &rpc->request;

    req->args.ioctl.cmd = (u32)cmd;
    req->args.ioctl.outPtr = (output) ? (u8*)OSVirtualToPhysical(output) : 0;
    req->args.ioctl.outLen = outputLen;
    req->args.ioctl.inPtr = (input) ? (u8*)OSVirtualToPhysical(input) : 0;
    req->args.ioctl.inLen = inputLen;

    DCFlushRange(input, inputLen);
    DCFlushRange(output, outputLen);

error:
    return ret;
}

/*
 * IOS_IoctlAsync
 *
 * Sends an ioctl with one input and one output buffer; cb runs when IOS replies.
 */
IOSError IOS_IoctlAsync(IOSFd fd, s32 cmd, void* input, u32 inputLen, void* output, u32 outputLen, IOSIpcCb cb,
                        void* cbArg) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 6, cb, cbArg, &rpc);

    if (ret != 0) {
        goto err;
    }

    ret = __ios_Ioctl(rpc, cmd, input, inputLen, output, outputLen);

    if (ret != 0) {
        goto err;
    }

    ret = __ios_Ipc2(rpc, cb);

err:
    return ret;
} // 0x80242CF0

/*
 * IOS_Ioctl
 *
 * Sends an ioctl with one input and one output buffer and returns its result.
 */
IOSError IOS_Ioctl(IOSFd fd, s32 cmd, void* input, u32 inputLen, void* output, u32 outputLen) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 6, 0, 0, &rpc);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ioctl(rpc, cmd, input, inputLen, output, outputLen);

    if (ret != 0) {
        goto error;
    }

    ret = __ios_Ipc2(rpc, 0);

error:
    return ret;
} // 0x80242E30

/*
 * __ios_Ioctlv
 *
 * Fills in the command and vector of an ioctlv request, converting every buffer address to physical.
 */
static IOSError __ios_Ioctlv(IOSRpcRequest* rpc, s32 cmd, u32 readCount, u32 writeCount, IOSIoVector* vect) {
    IOSError ret = 0;
    IOSResourceRequest* req;
    IOSResourceIoctlv* v;
    u32 i, j;

    if (!rpc) {
        ret = -4;
        goto err;
    }

    req = &rpc->request;
    req->args.ioctlv.cmd = (u32)cmd;
    req->args.ioctlv.readCount = readCount;
    req->args.ioctlv.writeCount = writeCount;
    req->args.ioctlv.vector = vect;

    v = &req->args.ioctlv;

    for (i = 0, j = v->readCount; i < req->args.ioctlv.writeCount; ++i) {
        DCFlushRange(v->vector[j + i].base, v->vector[j + i].length);
        v->vector[j + i].base = (v->vector[j + i].base) ? (u8*)OSVirtualToPhysical(v->vector[j + i].base) : 0;
    }

    for (i = 0; i < req->args.ioctlv.readCount; ++i) {
        DCFlushRange(v->vector[i].base, v->vector[i].length);
        v->vector[i].base = (v->vector[i].base) ? (u8*)OSVirtualToPhysical(v->vector[i].base) : 0;
    }

    DCFlushRange(&v->vector[0], (v->readCount + v->writeCount) * sizeof(IOSIoVector));
    req->args.ioctlv.vector = (vect) ? (IOSIoVector*)OSVirtualToPhysical(vect) : 0;

err:
    return ret;
} // 0x80242F60

/*
 * IOS_IoctlvAsync
 *
 * Sends a vectored ioctl; cb runs when IOS replies.
 */
IOSError IOS_IoctlvAsync(IOSFd fd, s32 cmd, u32 readCount, u32 writeCount, IOSIoVector* vect, IOSIpcCb cb, void* cbArg) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 7, cb, cbArg, &rpc);

    if (ret != 0) {
        goto err;
    }

    ret = __ios_Ioctlv(rpc, cmd, readCount, writeCount, vect);

    if (ret != 0) {
        goto err;
    }

    ret = __ios_Ipc2(rpc, cb);

err:
    return ret;
} // 0x802430A0

/*
 * IOS_Ioctlv
 *
 * Sends a vectored ioctl (readCount input vectors, then writeCount output vectors) and returns its result.
 */
IOSError IOS_Ioctlv(IOSFd fd, s32 cmd, u32 readCount, u32 writeCount, IOSIoVector* vect) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;

    ret = __ios_Ipc1(fd, 7, 0, 0, &rpc);

    if (ret != 0) {
        goto err;
    }

    ret = __ios_Ioctlv(rpc, cmd, readCount, writeCount, vect);

    if (ret != 0) {
        goto err;
    }

    ret = __ios_Ipc2(rpc, 0);

err:
    return ret;
} // 0x80243190

/*
 * IOS_IoctlvReboot
 *
 * Sends the vectored ioctl that makes IOS reload; waits on the saved copy of the request, which survives the reload.
 */
IOSError IOS_IoctlvReboot(IOSFd fd, s32 cmd, u32 readCount, u32 writeCount, IOSIoVector* vect) {
    IOSRpcRequest* rpc;
    IOSError ret = 0;
    u32 inten;
    IOSResourceRequest* req;

    inten = OSDisableInterrupts();

    if (__relnchFl) {
        OSRestoreInterrupts(inten);
        ret = -10;
        goto finish;
    }

    __relnchFl = 1;
    OSRestoreInterrupts(inten);

    ret = __ios_Ipc1(fd, 7, 0, 0, &rpc);

    if (ret != 0) {
        goto err;
    }

    __relnchRpcSave = rpc;
    rpc->relaunch_flag = 1;

    ret = __ios_Ioctlv(rpc, cmd, readCount, writeCount, vect);

    if (ret != 0) {
        goto err;
    }

    memcpy(&__rpcBuf, rpc, sizeof(IOSRpcRequest));
    __relnchRpc = (IOSRpcRequest*)&__rpcBuf;
    req = &rpc->request;

    OSInitThreadQueue(&__relnchRpc->thread_queue);
    DCFlushRange(req, sizeof(*req));

    inten = OSDisableInterrupts();
    ret = __ipcQueueRequest(req);

    if (ret != 0) {
        OSRestoreInterrupts(inten);
        goto err;
    }

    if (__mailboxAck > 0) {
        __ipcSendRequest();
    }

    OSSleepThread(&__relnchRpc->thread_queue);
    OSRestoreInterrupts(inten);
    ret = (&__relnchRpc->request)->status;

err:
    __relnchFl = 0;
    __relnchRpcSave = NULL;

    if (rpc && (ret != 0)) {
        ipcFree(rpc);
    }

finish:
    return ret;
} // 0x80243270
