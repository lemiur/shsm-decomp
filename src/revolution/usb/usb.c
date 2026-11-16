// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/usb/usb.c
// Reference: zeldaret/tp libs/revolution/src/usb/usb.c
// Reference: PrimeDecomp/sdk src/revolution/usb/usb.c
//
// IOS USB client: opens devices under /dev/usb and issues interrupt, bulk and control transfers through IOS ioctls,
// with the IPC buffers taken from a private IOS heap at the bottom of the IPC arena.
//
// Functions whose closing comment says "no copy in retail" have no out-of-line copy in the binary (removed by the
// linker); their strings remain in the unit's .data. They keep PrimeDecomp/sdk's bodies in the SDK order, as SDK C
// units do (AGENTS.md, "Dead-stripped functions").
//
// Parked (NonMatching) pending the user's ruling on invented dead-stripped bodies that carry retail strings:
// IUSB_ReadBlkMsg, IUSB_WriteBlkMsg and IUSB_RegisterInsertionNotifyWithIdAsync have no reference body and are
// written here only so that their five strings land in .data (open item, docs/exceptions.md "sdk-usb-1 rulings").
#include <revolution/ipc/ipcclt.h>
#include <revolution/ipc/ipcMain.h>
#include <revolution/ipc/memory.h>
#include <revolution/ios/iosrestypes.h>
#include <revolution/os.h>
#include <revolution/usb/__usb.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Size of the library's IOS heap, carved from the bottom of the IPC arena. */
#define IPC_ARENA_SIZE 0x4000

/* Largest transfer the short (16-bit length) bulk ioctl takes; longer ones use the long bulk ioctl. */
#define USB_SHORT_BLK_LIMIT 0xFFFF

/* IOS USB ioctl numbers (names invented). */
enum {
    USB_IOCTL_CTRLMSG = 0,
    USB_IOCTL_BLKMSG = 1,
    USB_IOCTL_INTRMSG = 2,
    USB_IOCTL_SUSPEND = 5,
    USB_IOCTL_RESUME = 6,
    USB_IOCTL_ISOMSG = 9,
    USB_IOCTL_LONGBLKMSG = 10,
    USB_IOCTL_GETDEVLIST = 12,
    USB_IOCTL_GETRHDESCA = 15,
    USB_IOCTL_GETRHPORTSTATUS = 20,
    USB_IOCTL_SETRHPORTSTATUS = 25,
    USB_IOCTL_DEVREMOVALHOOK = 26,
    USB_IOCTL_DEVINSERTHOOK = 27,
    USB_IOCTL_DEVCLASSCHANGE = 28,
    USB_IOCTL_RESET = 29
};

/* USB standard request GET_DESCRIPTOR and its descriptor types (names invented). */
#define USB_REQTYPE_DEV_TO_HOST 0x80
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_DT_DEVICE 0x01
#define USB_DT_STRING 0x03

static IOSHeapId hId = -1;
void* lo = NULL;
void* hi = NULL;

static u8 s_usb_log = 0;
static u8 s_usb_err = 1;

/*
 * _usb_enable_log
 *
 * Turns on the library's trace output.
 */
void _usb_enable_log(void)
{
    s_usb_log = 1;
} // no copy in retail (dead-stripped)

/*
 * _usb_disable_log
 *
 * Turns off the library's trace output.
 */
void _usb_disable_log(void)
{
    s_usb_log = 0;
} // no copy in retail (dead-stripped)

/*
 * USB_LOG
 *
 * Prints a trace line when tracing is on.
 */
void USB_LOG(const char* str, ...)
{
    va_list vlist;

    if (s_usb_log) {
        OSReport("USB: ");
        va_start(vlist, str);
        vprintf(str, vlist);
        va_end(vlist);
    }
} // 0x8027C0A0

/*
 * USB_ERR
 *
 * Prints an error line when error output is on.
 */
void USB_ERR(const char* str, ...)
{
    va_list vlist;

    if (s_usb_err) {
        OSReport("USB ERR: ");
        va_start(vlist, str);
        vprintf(str, vlist);
        va_end(vlist);
    }
} // 0x8027C150

/*
 * IOSAlloc
 *
 * Allocates a 32-byte aligned IPC buffer from the library's heap.
 */
static void* IOSAlloc(u32 size)
{
    void* rv;

    if ((rv = iosAllocAligned(hId, size, 32)) == 0) {
        USB_ERR("iosAllocAligned(%d, %u) failed: %d\n", hId, size, rv);
    }

    return rv;
} // no copy in retail (inlined)

/*
 * IOSFree
 *
 * Returns an IPC buffer to the library's heap; NULL is ignored.
 */
static void IOSFree(void* addr)
{
    IOSError rv;

    if (addr) {
        if ((rv = iosFree(hId, addr)) < 0) {
            USB_ERR("iosFree(%d, 0x%x) failed: %d\n", hId, addr, rv);
        }
    }
} // no copy in retail (inlined)

/*
 * IUSB_OpenLib
 *
 * Creates the library's IOS heap, taking its memory from the IPC arena the first time.
 */
IOSError IUSB_OpenLib(void)
{
    IOSError rc = IOS_ERROR_OK;
    BOOL enabled = OSDisableInterrupts();

    if (hId != -1) {
        USB_LOG("Library is already initialized. Heap Id = %d\n", hId);
        goto out;
    }

    if (lo == NULL) {
        lo = IPCGetBufferLo();
        hi = IPCGetBufferHi();
        USB_LOG("iusb size: %d lo: %x hi: %x\n", sizeof(iusb_ctxt), lo, hi);

        if ((u32)lo + ROUNDUP(IPC_ARENA_SIZE) > (u32)hi) {
            USB_ERR("Not enough IPC arena\n");
            rc = IOS_ERROR_FAIL_ALLOC;
            goto out;
        }

        IPCSetBufferLo((void*)((u32)lo + ROUNDUP(IPC_ARENA_SIZE)));
    }

    hId = iosCreateHeap(lo, IPC_ARENA_SIZE);

    if (hId < 0) {
        USB_ERR("Not enough heaps\n");
        rc = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

out:
    OSRestoreInterrupts(enabled);
    return rc;
} // 0x8027C200

/*
 * IUSB_CloseLib
 *
 * Nothing to release: the heap stays for the next IUSB_OpenLib.
 */
IOSError IUSB_CloseLib(void)
{
    return IOS_ERROR_OK;
} // 0x8027C300

/*
 * _intrBlkCtrlIsoCb
 *
 * IOS completion of an asynchronous request: frees the request's IPC buffers, calls the caller's callback (or the
 * isochronous one) and frees the request.
 */
static IOSError _intrBlkCtrlIsoCb(IOSError ret, void* ctxt)
{
    u32 i;
    IOSError rv = ret;
    iusb_ctxt* req = (iusb_ctxt*)ctxt;

    USB_LOG("_intrBlkCtrlIsoCb returned: %d\n", rv);
    USB_LOG("_intrBlkCtrlIsoCb: nclean = %d\n", req->nclean);

    if (req->nclean != 7 && req->nclean != 3 && req->nclean != 0 && req->nclean != 4 && req->nclean != 2) {
        USB_ERR("__intrBlkCtrlIsoCb: got invalid nclean\n");
    } else {
        for (i = 0; i < req->nclean; ++i) {
            USB_LOG("Freeing clean[%d] = %x\n", i, req->clean[i]);
            IOSFree(req->clean[i]);
        }
        req->nclean = 0;
    }

    USB_LOG("cb = %x cbArg = %x\n", req->cb, req->cbArg);

    if (req->cb) {
        req->cb(ret, req->cbArg);
    } else if (req->icb) {
        USB_LOG("calling iso callback\n");
        req->icb(ret, req->xfer, req->cbArg);
    }

    IOSFree(req);
    return rv;
} // 0x8027C310

/*
 * IUSB_OpenDeviceIds
 *
 * Opens /dev/usb/<did>/<vid>/<pid> and stores the descriptor in *fd.
 */
IOSError IUSB_OpenDeviceIds(const char* did, u16 vid, u16 pid, IOSFd* fd)
{
    IOSError rv = IOS_ERROR_OK;
    iusb_ctxt* req = NULL;

    if (fd == NULL) {
        rv = IOS_ERROR_INVALID;
        goto out;
    }

    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (req == NULL) {
        USB_ERR("OpenDeviceIds: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memset(req, 0, sizeof(iusb_ctxt));
    snprintf(req->u.path, sizeof(req->u.path), "/dev/usb/%s/%x/%x", did, vid, pid);
    USB_LOG("OpenDevice - %s\n", req->u.path);
    rv = IOS_Open(req->u.path, 0);
    USB_LOG("OpenDevice returned: %d\n", rv);
    *fd = rv;

out:
    IOSFree(req);
    return rv;
} // 0x8027C4B0

/*
 * IUSB_OpenDeviceIdsAsync
 *
 * Asynchronous IUSB_OpenDeviceIds: the descriptor reaches the callback as its result.
 */
IOSError IUSB_OpenDeviceIdsAsync(const char* did, u16 vid, u16 pid, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv = IOS_ERROR_OK;
    iusb_ctxt* req;

    USB_LOG("OpenDevice\n");
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (req == NULL) {
        USB_ERR("OpenDeviceIdsAsync: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memset(req, 0, sizeof(iusb_ctxt));

    req->cb = cb;
    req->cbArg = cbArg;
    req->nclean = 0;

    snprintf(req->u.path, sizeof(req->u.path), "/dev/usb/%s/%x/%x", did, vid, pid);
    USB_LOG("OpenDevice - %s\n", req->u.path);
    rv = IOS_OpenAsync(req->u.path, 0, _intrBlkCtrlIsoCb, req);
    USB_LOG("OpenDevice returned: %d\n", rv);
    if (rv < 0) {
        IOSFree(req);
        goto out;
    }

out:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_CloseDevice
 *
 * Closes a device descriptor.
 */
IOSError IUSB_CloseDevice(IOSFd fd)
{
    IOSError rv = IOS_ERROR_OK;

    USB_LOG("CloseDevice\n");
    rv = IOS_Close(fd);
    USB_LOG("CloseDevice returned: %d\n", rv);

    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_CloseDeviceAsync
 *
 * Asynchronous IUSB_CloseDevice.
 */
IOSError IUSB_CloseDeviceAsync(IOSFd fd, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv = IOS_ERROR_OK;
    iusb_ctxt* req;

    USB_LOG("CloseDevice\n");
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (req == NULL) {
        USB_ERR("CloseDeviceAsync: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memset(req, 0, sizeof(iusb_ctxt));

    req->cb = cb;
    req->cbArg = cbArg;
    req->nclean = 0;

    rv = IOS_CloseAsync(fd, _intrBlkCtrlIsoCb, req);
    USB_LOG("CloseDevice returned: %d\n", rv);
    if (rv < 0) {
        IOSFree(req);
        goto out;
    }

out:
    return rv;
} // 0x8027C600

/*
 * __openDevice
 *
 * Opens a host controller path through an IPC buffer.
 */
static IOSFd __openDevice(const char* path)
{
    IOSFd rv;
    char* pathBuf;
    iusb_ctxt* ctx = IOSAlloc(ROUNDUP(sizeof(iusb_ctxt)));

    if (ctx == NULL) {
        USB_ERR("openDevice: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto done;
    }

    pathBuf = ctx->u.path;
    strncpy(pathBuf, path, 64);
    rv = IOS_Open(pathBuf, 0);
    IOSFree(ctx);

done:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_GetDeviceList
 *
 * Lists up to maxDev devices of a class attached to a host controller.
 */
IOSError IUSB_GetDeviceList(const char* path, USBDeviceInfo* deviceList, u8 maxDev, u8 deviceClass, u8* numDev)
{
    IOSError rv = IOS_ERROR_OK;
    IOSIoVector* vector;
    u8* smaxDev;
    u8* sclass;
    u8* snumDev;
    IOSFd fd;

    if ((u32)deviceList & 0x1F) {
        rv = IOS_ERROR_INVALID;
        goto out;
    }
    if ((fd = __openDevice(path)) < 0) {
        rv = fd;
        goto out;
    }

    vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 4);
    smaxDev = IOSAlloc(ROUNDUP(sizeof(u8)));
    sclass = IOSAlloc(ROUNDUP(sizeof(u8)));
    snumDev = IOSAlloc(ROUNDUP(sizeof(u8)));
    if (0 == smaxDev || 0 == sclass || 0 == snumDev || 0 == vector) {
        USB_ERR("getDeviceList: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    *smaxDev = maxDev;
    *sclass = deviceClass;
    *snumDev = 0;

    vector[0].base = smaxDev;
    vector[0].length = sizeof(u8);
    vector[1].base = sclass;
    vector[1].length = sizeof(u8);
    vector[2].base = snumDev;
    vector[2].length = sizeof(u8);
    vector[3].base = (u8*)deviceList;
    vector[3].length = sizeof(USBDeviceInfo) * maxDev;

    DCInvalidateRange(deviceList, sizeof(USBDeviceInfo) * maxDev);
    DCInvalidateRange(snumDev, ROUNDUP(sizeof(u8)));
    DCFlushRange(sclass, ROUNDUP(sizeof(u8)));
    DCFlushRange(smaxDev, ROUNDUP(sizeof(u8)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 4);

    rv = IOS_Ioctlv(fd, USB_IOCTL_GETDEVLIST, 2, 2, vector);
    *numDev = *snumDev;
    IOS_Close(fd);

out:
    // BUG: the early exits free the buffers before they are allocated.
    IOSFree(snumDev);
    IOSFree(smaxDev);
    IOSFree(sclass);
    IOSFree(vector);
    return rv;
} // no copy in retail (dead-stripped)

/*
 * __LongBlkMsgInt
 *
 * A bulk transfer longer than the short bulk ioctl takes (32-bit length), synchronous or asynchronous.
 */
static IOSError __LongBlkMsgInt(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg, u8 async)
{
    IOSError rv = IOS_ERROR_OK;
    IOSIoVector* vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 3);
    u8* sep = IOSAlloc(ROUNDUP(sizeof(u8)));
    u32* sbuflen = IOSAlloc(ROUNDUP(sizeof(u32)));

    if (0 == vector || 0 == sep || 0 == sbuflen) {
        USB_ERR("__LongBlkMsgInt: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto clean;
    }

    *sep = (u8)ep;
    *sbuflen = buflen;
    vector[0].base = sep;
    vector[0].length = sizeof(u8);
    vector[1].base = (u8*)sbuflen;
    vector[1].length = sizeof(u32);
    vector[2].base = (u8*)buf;
    vector[2].length = buflen;

    DCFlushRange(sep, ROUNDUP(sizeof(u8)));
    DCFlushRange(sbuflen, ROUNDUP(sizeof(u32)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 3);

    if (0 == async) {
        rv = IOS_Ioctlv(fd, USB_IOCTL_LONGBLKMSG, 2, 1, vector);
        USB_LOG("Long bulk ioctl returned: %d\n", rv);
        goto clean;
    } else {
        iusb_ctxt* req = IOSAlloc(ROUNDUP(sizeof(iusb_ctxt)));
        if (0 == req) {
            USB_ERR("LongBlkMsgInt (async): Not enough memory\n");
            rv = IOS_ERROR_FAIL_ALLOC;
            goto clean;
        }

        memset(req, 0, sizeof(iusb_ctxt));
        req->cb = cb;
        req->cbArg = cbArg;
        USB_LOG("longblkmsg: cb = 0x%x cbArg = 0x%x\n", req->cb, req->cbArg);
        req->nclean = 3;
        req->clean[0] = sep;
        req->clean[1] = sbuflen;
        req->clean[2] = vector;
        req->u.reqBuf.data = buf;
        req->u.reqBuf.wLength = (u16)buflen;

        rv = IOS_IoctlvAsync(fd, USB_IOCTL_LONGBLKMSG, 2, 1, vector, _intrBlkCtrlIsoCb, req);
        if (rv >= 0) {
            goto out;
        }

        if (rv == IOS_ERROR_FAIL_ALLOC) {
            OSReport("%s: IoctlvAsync returned error %d\n", __FUNCTION__, rv);
        }

        IOSFree(req);
        goto clean;
    }

clean:
    IOSFree(sep);
    IOSFree(sbuflen);
    IOSFree(vector);
out:
    return rv;
} // 0x8027C730

/*
 * __IntrBlkMsgInt
 *
 * An interrupt or short bulk transfer (ioctl typ, 16-bit length), synchronous or asynchronous.
 */
static IOSError __IntrBlkMsgInt(IOSFd fd, u32 ep, u32 buflen, char* buf, u8 typ, USBCallbackFunc cb, void* cbArg,
                                u8 async)
{
    IOSError rv = IOS_ERROR_OK;
    IOSIoVector* vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 3);
    u8* sep = IOSAlloc(ROUNDUP(sizeof(u8)));
    u16* sbuflen = IOSAlloc(ROUNDUP(sizeof(u16)));

    if (0 == vector || 0 == sep || 0 == sbuflen) {
        USB_ERR("__IntrBlkMsgInt: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto clean;
    }

    *sep = (u8)ep;
    *sbuflen = (u16)buflen;
    vector[0].base = sep;
    vector[0].length = sizeof(u8);
    vector[1].base = (u8*)sbuflen;
    vector[1].length = sizeof(u16);
    vector[2].base = (u8*)buf;
    vector[2].length = buflen;

    DCFlushRange(sep, ROUNDUP(sizeof(u8)));
    DCFlushRange(sbuflen, ROUNDUP(sizeof(u16)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 3);

    if (0 == async) {
        rv = IOS_Ioctlv(fd, typ, 2, 1, vector);
        USB_LOG("intr/blk ioctl returned: %d\n", rv);
        goto clean;
    } else {
        iusb_ctxt* req = IOSAlloc(ROUNDUP(sizeof(iusb_ctxt)));
        if (0 == req) {
            USB_ERR("IntBlkMsgInt (async): Not enough memory\n");
            rv = IOS_ERROR_FAIL_ALLOC;
            goto clean;
        }

        memset(req, 0, sizeof(iusb_ctxt));
        req->cb = cb;
        req->cbArg = cbArg;
        USB_LOG("intrblkmsg: cb = 0x%x cbArg = 0x%x\n", req->cb, req->cbArg);
        req->nclean = 3;
        req->clean[0] = sep;
        req->clean[1] = sbuflen;
        req->clean[2] = vector;
        req->u.reqBuf.data = buf;
        req->u.reqBuf.wLength = (u16)buflen;

        rv = IOS_IoctlvAsync(fd, typ, 2, 1, vector, _intrBlkCtrlIsoCb, req);
        if (rv >= 0) {
            goto out;
        }

        if (rv == IOS_ERROR_FAIL_ALLOC) {
            OSReport("%s: IoctlvAsync returned error %d\n", __FUNCTION__, rv);
        }

        IOSFree(req);
        goto clean;
    }

clean:
    IOSFree(sep);
    IOSFree(sbuflen);
    IOSFree(vector);
out:
    return rv;
} // 0x8027CA90

/*
 * IUSB_ReadIntrMsg
 *
 * Synchronous interrupt read.
 */
IOSError IUSB_ReadIntrMsg(IOSFd fd, u32 ep, u32 buflen, char* buf)
{
    IOSError rv;

    DCInvalidateRange(buf, buflen);
    rv = __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_INTRMSG, NULL, NULL, 0);

    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_ReadIntrMsgAsync
 *
 * Asynchronous interrupt read.
 */
IOSError IUSB_ReadIntrMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg)
{
    DCInvalidateRange(buf, buflen);
    return __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_INTRMSG, cb, cbArg, 1);
} // 0x8027CDF0

/*
 * IUSB_WriteIntrMsg
 *
 * Synchronous interrupt write.
 */
IOSError IUSB_WriteIntrMsg(IOSFd fd, u32 ep, u32 buflen, char* buf)
{
    DCFlushRange(buf, buflen);
    return __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_INTRMSG, NULL, NULL, 0);
} // no copy in retail (dead-stripped)

/*
 * IUSB_WriteIntrMsgAsync
 *
 * Asynchronous interrupt write.
 */
IOSError IUSB_WriteIntrMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg)
{
    DCFlushRange(buf, buflen);
    return __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_INTRMSG, cb, cbArg, 1);
} // no copy in retail (dead-stripped)

/*
 * IUSB_ReadBlkMsg
 *
 * Synchronous bulk read: the short bulk ioctl up to its 16-bit limit, the long one beyond.
 * No reference has this body: it is reconstructed from its two retail strings and IUSB_ReadBlkMsgAsync.
 */
IOSError IUSB_ReadBlkMsg(IOSFd fd, u32 ep, u32 buflen, char* buf)
{
    IOSError rv;

    DCInvalidateRange(buf, buflen);
    if (buflen <= USB_SHORT_BLK_LIMIT) {
        USB_LOG("calling short blk transfer fn: buflen = %u limit = %u\n", buflen, USB_SHORT_BLK_LIMIT);
        rv = __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_BLKMSG, NULL, NULL, 0);
    } else {
        USB_LOG("calling long blk transfer fn: buflen = %u limit = %u\n", buflen, USB_SHORT_BLK_LIMIT);
        rv = __LongBlkMsgInt(fd, ep, buflen, buf, NULL, NULL, 0);
    }

    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_ReadBlkMsgAsync
 *
 * Asynchronous bulk read: the short bulk ioctl up to its 16-bit limit, the long one beyond.
 */
IOSError IUSB_ReadBlkMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv;

    DCInvalidateRange(buf, buflen);
    if (buflen <= USB_SHORT_BLK_LIMIT) {
        rv = __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_BLKMSG, cb, cbArg, 1);
    } else {
        rv = __LongBlkMsgInt(fd, ep, buflen, buf, cb, cbArg, 1);
    }

    return rv;
} // 0x8027CE70

/*
 * IUSB_WriteBlkMsg
 *
 * Synchronous bulk write: the short bulk ioctl up to its 16-bit limit, the long one beyond.
 * No reference has this body: it is reconstructed from IUSB_ReadBlkMsg and IUSB_WriteBlkMsgAsync.
 */
IOSError IUSB_WriteBlkMsg(IOSFd fd, u32 ep, u32 buflen, char* buf)
{
    IOSError rv;

    DCFlushRange(buf, buflen);
    if (buflen <= USB_SHORT_BLK_LIMIT) {
        USB_LOG("calling short blk transfer fn: buflen = %u limit = %u\n", buflen, USB_SHORT_BLK_LIMIT);
        rv = __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_BLKMSG, NULL, NULL, 0);
    } else {
        USB_LOG("calling long blk transfer fn: buflen = %u limit = %u\n", buflen, USB_SHORT_BLK_LIMIT);
        rv = __LongBlkMsgInt(fd, ep, buflen, buf, NULL, NULL, 0);
    }

    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_WriteBlkMsgAsync
 *
 * Asynchronous bulk write: the short bulk ioctl up to its 16-bit limit, the long one beyond.
 */
IOSError IUSB_WriteBlkMsgAsync(IOSFd fd, u32 ep, u32 buflen, char* buf, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv;

    DCFlushRange(buf, buflen);
    if (buflen <= USB_SHORT_BLK_LIMIT) {
        rv = __IntrBlkMsgInt(fd, ep, buflen, buf, USB_IOCTL_BLKMSG, cb, cbArg, 1);
    } else {
        rv = __LongBlkMsgInt(fd, ep, buflen, buf, cb, cbArg, 1);
    }

    return rv;
} // 0x8027CF10

/*
 * __CtrlMsgInt
 *
 * A control transfer: the setup packet's fields go to IOS in separate IPC buffers (the 16-bit ones little-endian),
 * synchronous or asynchronous.
 */
static IOSError __CtrlMsgInt(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 length, char* data,
                             USBCallbackFunc cb, void* cbArg, u8 async)
{
    IOSError rv = IOS_ERROR_OK;
    IOSIoVector* vector;
    u8* sreqType;
    u8* srequest;
    u8* sep;
    u16* svalue;
    u16* sindex;
    u16* slength;

    if ((data == 0 && length != 0) || ((u32)data & (u32)(32 - 1))) {
        rv = IOS_ERROR_INVALID;
        USB_ERR("ctrlmsg: bad data buffer\n");
        goto out;
    }

    vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 7);
    sreqType = IOSAlloc(ROUNDUP(sizeof(u8)));
    srequest = IOSAlloc(ROUNDUP(sizeof(u8)));
    sep = IOSAlloc(ROUNDUP(sizeof(u8)));
    svalue = IOSAlloc(ROUNDUP(sizeof(u16)));
    sindex = IOSAlloc(ROUNDUP(sizeof(u16)));
    slength = IOSAlloc(ROUNDUP(sizeof(u16)));

    if (0 == sreqType || 0 == srequest || 0 == sep || 0 == svalue || 0 == sindex || 0 == slength ||
        0 == vector) {
        USB_ERR("Ctrl Msg: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto clean;
    }

    *sreqType = reqType;
    *srequest = request;
    *svalue = SWAP16(value);
    *sindex = SWAP16(index);
    *slength = SWAP16(length);
    *sep = 0;

    vector[0].base = sreqType;
    vector[0].length = sizeof(u8);
    vector[1].base = srequest;
    vector[1].length = sizeof(u8);
    vector[2].base = (u8*)svalue;
    vector[2].length = sizeof(u16);
    vector[3].base = (u8*)sindex;
    vector[3].length = sizeof(u16);
    vector[4].base = (u8*)slength;
    vector[4].length = sizeof(u16);
    vector[5].base = sep;
    vector[5].length = sizeof(u8);
    vector[6].base = (u8*)data;
    vector[6].length = length;

    DCFlushRange(sreqType, ROUNDUP(sizeof(u8)));
    DCFlushRange(srequest, ROUNDUP(sizeof(u8)));
    DCFlushRange(sep, ROUNDUP(sizeof(u8)));
    DCFlushRange(svalue, ROUNDUP(sizeof(u16)));
    DCFlushRange(sindex, ROUNDUP(sizeof(u16)));
    DCFlushRange(slength, ROUNDUP(sizeof(u16)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 7);

    if (0 == async) {
        rv = IOS_Ioctlv(fd, USB_IOCTL_CTRLMSG, 6, 1, vector);
        goto clean;
    } else {
        iusb_ctxt* req = IOSAlloc(ROUNDUP(sizeof(iusb_ctxt)));
        if (0 == req) {
            USB_ERR("CtrlMsgInt (async): Not enough memory\n");
            rv = IOS_ERROR_FAIL_ALLOC;
            goto clean;
        }

        memset(req, 0, sizeof(iusb_ctxt));
        req->cb = cb;
        req->cbArg = cbArg;
        USB_LOG("ctrlmsgint: cb = 0x%x cbArg = 0x%x\n", req->cb, req->cbArg);
        req->nclean = 7;
        req->clean[0] = sreqType;
        req->clean[1] = srequest;
        req->clean[2] = svalue;
        req->clean[3] = sindex;
        req->clean[4] = slength;
        req->clean[5] = sep;
        req->clean[6] = vector;
        req->u.reqBuf.data = data;
        req->u.reqBuf.wLength = length;

        rv = IOS_IoctlvAsync(fd, USB_IOCTL_CTRLMSG, 6, 1, vector, _intrBlkCtrlIsoCb, req);
        USB_LOG("Ctrl Msg async returned: %d\n", rv);

        if (rv < 0) {
            IOSFree(req);
            goto clean;
        }

        goto out;
    }

clean:
    IOSFree(sreqType);
    IOSFree(srequest);
    IOSFree(svalue);
    IOSFree(sindex);
    IOSFree(slength);
    IOSFree(sep);
    IOSFree(vector);
out:
    return rv;
} // 0x8027CFB0

/*
 * IUSB_ReadCtrlMsg
 *
 * Synchronous device-to-host control transfer.
 */
IOSError IUSB_ReadCtrlMsg(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf)
{
    DCInvalidateRange(buf, buflen);
    return __CtrlMsgInt(fd, reqType, request, value, index, buflen, buf, NULL, NULL, 0);
} // no copy in retail (dead-stripped)

/*
 * IUSB_ReadCtrlMsgAsync
 *
 * Asynchronous device-to-host control transfer.
 */
IOSError IUSB_ReadCtrlMsgAsync(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf,
                               USBCallbackFunc cb, void* cbArg)
{
    DCInvalidateRange(buf, buflen);
    return __CtrlMsgInt(fd, reqType, request, value, index, buflen, buf, cb, cbArg, 1);
} // no copy in retail (dead-stripped)

/*
 * IUSB_WriteCtrlMsg
 *
 * Synchronous host-to-device control transfer.
 */
IOSError IUSB_WriteCtrlMsg(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf)
{
    DCFlushRange(buf, buflen);
    return __CtrlMsgInt(fd, reqType, request, value, index, buflen, buf, NULL, NULL, 0);
} // no copy in retail (dead-stripped)

/*
 * IUSB_WriteCtrlMsgAsync
 *
 * Asynchronous host-to-device control transfer.
 */
IOSError IUSB_WriteCtrlMsgAsync(IOSFd fd, u8 reqType, u8 request, u16 value, u16 index, u16 buflen, char* buf,
                                USBCallbackFunc cb, void* cbArg)
{
    DCFlushRange(buf, buflen);
    return __CtrlMsgInt(fd, reqType, request, value, index, buflen, buf, cb, cbArg, 1);
} // 0x8027D560

/*
 * unicode2ascii
 *
 * Converts a USB string descriptor (UTF-16LE) in place to ASCII, '?' for characters outside it; returns the length
 * or -1 when the buffer is not a string descriptor.
 */
static s8 unicode2ascii(char* tbuf, int buflen)
{
    char buf[128];
    s8 di, si;

    if (tbuf[1] != USB_DT_STRING) {
        di = -1;
        goto out;
    }

    for (di = 0, si = 2; si < tbuf[0] && si < buflen; si += 2) {
        if (di >= (sizeof(buf) - 1)) {
            break;
        }
        if (tbuf[si + 1]) {
            buf[di++] = '?';
        } else {
            buf[di++] = tbuf[si];
        }
    }

    buf[di] = 0;
    memcpy(tbuf, buf, (u32)di);

out:
    return di;
} // no copy in retail (dead-stripped)

/*
 * _GetStrCb
 *
 * Completion of IUSB_GetAsciiStrAsync: converts the descriptor to ASCII and calls the caller's callback.
 */
static void _GetStrCb(IOSError ret, void* ctxt)
{
    IOSError rv = ret;
    iusb_ctxt* req = (iusb_ctxt*)ctxt;
    char* buf;
    u16 buflen;
    s8 len;

    USB_LOG("GetStrCb returned: %d\n", rv);
    if (rv <= 0) {
        goto out;
    }

    buf = req->u.reqBuf.data;
    buflen = req->u.reqBuf.wLength;
    USB_LOG("GetStrCb: buf = 0x%x buflen = %u\n", buf, buflen);
    if ((len = unicode2ascii(buf, buflen)) < 0) {
        USB_ERR("Failed to convert buffer from unicode 2 ascii\n");
    } else {
        buf[len] = '\0';
    }

out:
    if (req->cb) {
        USB_LOG("calling cb 0x%x with arg 0x%x\n", req->cb, req->cbArg);
        req->cb(ret, req->cbArg);
    }
    IOSFree(req);
} // no copy in retail (dead-stripped)

/*
 * IUSB_GetAsciiStr
 *
 * Reads string descriptor index in language langId and converts it to ASCII.
 */
IOSError IUSB_GetAsciiStr(IOSFd fd, u8 ep, u16 index, u16 langId, char* buf, u16 buflen)
{
    IOSError rv = IOS_ERROR_OK;
    s8 len;

    DCInvalidateRange(buf, (u32)buflen);
    rv = __CtrlMsgInt(fd, USB_REQTYPE_DEV_TO_HOST, USB_REQ_GET_DESCRIPTOR, (u16)((USB_DT_STRING << 8) + index), langId,
                      buflen, buf, NULL, NULL, 0);
    if (rv <= 0) {
        USB_ERR("Failed __CtrlMsg: %d", rv);
        goto out;
    }
    if ((len = unicode2ascii(buf, buflen)) < 0) {
        USB_ERR("Failed to convert unicode 2 ascii\n");
    } else {
        buf[len] = '\0';
    }

out:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_GetAsciiStrAsync
 *
 * Asynchronous IUSB_GetAsciiStr.
 */
IOSError IUSB_GetAsciiStrAsync(IOSFd fd, u8 ep, u16 index, u16 langId, char* buf, u16 buflen, USBCallbackFunc cb,
                               void* cbArg)
{
    IOSError rv = IOS_ERROR_OK;
    iusb_ctxt* req;

    USB_LOG("GetStr - _GetStrCb\n");
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (req == NULL) {
        USB_ERR(" GetAsciiStrAsync: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memset(req, 0, sizeof(iusb_ctxt));

    req->cb = cb;
    req->cbArg = cbArg;
    req->nclean = 0;
    req->u.reqBuf.data = buf;
    req->u.reqBuf.wLength = buflen;
    DCInvalidateRange(buf, buflen);
    rv = __CtrlMsgInt(fd, USB_REQTYPE_DEV_TO_HOST, USB_REQ_GET_DESCRIPTOR, (u16)((USB_DT_STRING << 8) + index), langId,
                      buflen, buf, _GetStrCb, req, 1);
    if (rv < 0) {
        USB_ERR("__CtrlMsgInt failed %d\n", rv);
        IOSFree(req);
        goto out;
    }

out:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * _GetDescrCb
 *
 * Completion of IUSB_GetDevDescrAsync: copies the descriptor out and calls the caller's callback.
 */
static void _GetDescrCb(IOSError ret, void* ctxt)
{
    IOSError rv = ret;
    iusb_ctxt* req = (iusb_ctxt*)ctxt;
    USB_DevDescr* des;

    USB_LOG("GetDescrCb returned: %d\n", rv);
    if (rv <= 0) {
        goto out;
    }

    des = req->spare;
    *des = req->u.des;

out:
    if (req->cb) {
        req->cb(ret, req->cbArg);
    }
    IOSFree(req);
} // no copy in retail (dead-stripped)

/*
 * IUSB_GetDevDescr
 *
 * Reads the device descriptor.
 */
IOSError IUSB_GetDevDescr(IOSFd fd, USB_DevDescr* des)
{
    IOSError rv = IOS_ERROR_OK;
    iusb_ctxt* req = NULL;

    USB_LOG("GetDevDescr\n");
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (req == NULL) {
        USB_ERR("GetDevDescr: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memset(req, 0, sizeof(iusb_ctxt));
    DCInvalidateRange(&req->u.des, (u32)sizeof(*des));
    rv = __CtrlMsgInt(fd, USB_REQTYPE_DEV_TO_HOST, USB_REQ_GET_DESCRIPTOR, SWAP16(USB_DT_DEVICE), 0, sizeof(*des),
                      req->u.path, NULL, NULL, 0);
    if (rv < 0) {
        USB_ERR("Failed __CtrlMsg: %d", rv);
        goto out;
    }
    USB_LOG("GetDevDescr: %d\n", rv);
    *des = req->u.des;

out:
    IOSFree(req);
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_GetDevDescrAsync
 *
 * Asynchronous IUSB_GetDevDescr.
 */
IOSError IUSB_GetDevDescrAsync(IOSFd fd, USB_DevDescr* des, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv = IOS_ERROR_OK;
    iusb_ctxt* req;

    USB_LOG("GetDevDescr - _GetDescrCb\n");
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (req == NULL) {
        USB_ERR("GetDevDescrAsync: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto out;
    }

    memset(req, 0, sizeof(iusb_ctxt));

    req->cb = cb;
    req->cbArg = cbArg;
    req->nclean = 0;
    req->spare = des;

    rv = IUSB_ReadCtrlMsgAsync(fd, USB_REQTYPE_DEV_TO_HOST, USB_REQ_GET_DESCRIPTOR, SWAP16(USB_DT_DEVICE), 0,
                               sizeof(*des), req->u.path, _GetDescrCb, req);
    if (rv < 0) {
        IOSFree(req);
        goto out;
    }

out:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_SuspendDevice
 *
 * Suspends a device.
 */
IOSError IUSB_SuspendDevice(IOSFd fd)
{
    return IOS_Ioctl(fd, USB_IOCTL_SUSPEND, NULL, 0, NULL, 0);
} // no copy in retail (dead-stripped)

/*
 * IUSB_ResumeDevice
 *
 * Resumes a suspended device.
 */
IOSError IUSB_ResumeDevice(IOSFd fd)
{
    return IOS_Ioctl(fd, USB_IOCTL_RESUME, NULL, 0, NULL, 0);
} // no copy in retail (dead-stripped)

/*
 * IUSB_ResetDevice
 *
 * Resets a device.
 */
IOSError IUSB_ResetDevice(IOSFd fd)
{
    return IOS_Ioctl(fd, USB_IOCTL_RESET, NULL, 0, NULL, 0);
} // no copy in retail (dead-stripped)

/*
 * IUSB_GetRhDesca
 *
 * Reads the root hub's descriptor A register.
 */
IOSError IUSB_GetRhDesca(IOSFd fd, u32* desca)
{
    IOSError rv = IOS_ERROR_OK;
    u32* tmp;

    if (0 == desca) {
        rv = IOS_ERROR_INVALID;
        goto done;
    }

    tmp = IOSAlloc(ROUNDUP(sizeof(u32)));
    if (0 == tmp) {
        rv = IOS_ERROR_FAIL_ALLOC;
        goto done;
    }
    rv = IOS_Ioctl(fd, USB_IOCTL_GETRHDESCA, NULL, 0, tmp, sizeof(u32));
    *desca = *tmp;
    IOSFree(tmp);

done:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * __getsetRhPortStat
 *
 * Reads or writes a root hub port status through a two-vector ioctl.
 */
static IOSError __getsetRhPortStat(IOSFd fd, s32 cmd, u8* v0, u32 s0, u8* v1, u32 s1, u32 rd, u32 wr)
{
    IOSError rv;
    IOSIoVector* vector;

    if (0 == v0 || 0 == v1) {
        rv = IOS_ERROR_INVALID;
        goto done;
    }

    vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 2);
    if (0 == vector) {
        rv = IOS_ERROR_FAIL_ALLOC;
        goto done;
    }

    vector[0].base = v0;
    vector[0].length = s0;
    vector[1].base = v1;
    vector[1].length = s1;

    rv = IOS_Ioctlv(fd, cmd, rd, wr, vector);
    IOSFree(vector);

done:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_GetRhPortStatus
 *
 * Reads a root hub port's status.
 */
IOSError IUSB_GetRhPortStatus(IOSFd fd, u8 port, u32* stat)
{
    IOSError rv;
    u8* p;
    u32* s;

    p = IOSAlloc(ROUNDUP(sizeof(u8)));
    s = IOSAlloc(ROUNDUP(sizeof(u32)));
    if (0 == p || 0 == s) {
        rv = IOS_ERROR_FAIL_ALLOC;
        goto done;
    }

    *p = port;
    rv = __getsetRhPortStat(fd, USB_IOCTL_GETRHPORTSTATUS, p, sizeof(u8), (u8*)s, sizeof(u32), 1, 1);
    *stat = *s;

done:
    IOSFree(p);
    IOSFree(s);
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_SetRhPortStatus
 *
 * Writes a root hub port's status.
 */
IOSError IUSB_SetRhPortStatus(IOSFd fd, u8 port, u32 stat)
{
    IOSError rv;
    u8* p;
    u32* s;

    p = IOSAlloc(ROUNDUP(sizeof(u8)));
    s = IOSAlloc(ROUNDUP(sizeof(u32)));
    if (0 == p || 0 == s) {
        rv = IOS_ERROR_FAIL_ALLOC;
        goto done;
    }

    *p = port;
    *s = stat;
    rv = __getsetRhPortStat(fd, USB_IOCTL_SETRHPORTSTATUS, p, sizeof(u8), (u8*)s, sizeof(u32), 2, 0);

done:
    IOSFree(p);
    IOSFree(s);
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_DeviceRemovalNotifyAsync
 *
 * Calls cb when the device is removed.
 */
IOSError IUSB_DeviceRemovalNotifyAsync(IOSFd fd, IOSIpcCb cb, void* cbArg)
{
    IOSError rv;

    USB_LOG("DeviceRemovalNotifyAsync\n");
    rv = IOS_IoctlAsync(fd, USB_IOCTL_DEVREMOVALHOOK, NULL, 0, NULL, 0, cb, cbArg);
    return rv;
} // no copy in retail (dead-stripped)

/*
 * __checkIsoArgs
 *
 * Validates an isochronous transfer (one to eight packets of at most 1023 bytes) and sums its length.
 */
static IOSError __checkIsoArgs(IsoTransfer* xfer, u16* buflen)
{
    IOSError rv = IOS_ERROR_INVALID;
    u32 i;

    if (0 == xfer || 0 == xfer->buf || 0 == xfer->numPackets || xfer->numPackets > 8 || 0 == xfer->packets) {
        goto out;
    }

    *buflen = 0;
    for (i = 0; i < xfer->numPackets; ++i) {
        if (xfer->packets[i] > 1023) {
            USB_ERR("packet %u too big: %u\n", i, xfer->packets[i]);
            goto out;
        }
        *buflen += xfer->packets[i];
    }
    rv = IOS_ERROR_OK;

out:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_IsoMsgAsync
 *
 * Asynchronous isochronous transfer.
 */
IOSError IUSB_IsoMsgAsync(IOSFd fd, u8 ep, IsoTransfer* xfer, USBIsoCallbackFunc cb, void* cbArg)
{
    IOSError rv = IOS_ERROR_OK;
    u8* sep;
    u8* snumPackets;
    u16* sbuflen;
    IOSIoVector* vector;
    iusb_ctxt* req;
    u16 buflen;

    if (0 == ep || 0 == cb || (__checkIsoArgs(xfer, &buflen) < 0)) {
        USB_ERR("Invalid parameters for ISO transfer request\n");
        rv = IOS_ERROR_INVALID;
        goto out;
    }

    vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 5);
    sep = IOSAlloc(ROUNDUP(sizeof(u8)));
    sbuflen = IOSAlloc(ROUNDUP(sizeof(u16)));
    snumPackets = IOSAlloc(ROUNDUP(sizeof(u8)));
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (0 == vector || 0 == sep || 0 == sbuflen || 0 == req) {
        USB_ERR("IUSB_IsoMsgAsync: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        IOSFree(req);
        goto clean;
    }

    memset(req, 0, sizeof(iusb_ctxt));
    *sep = ep;
    *sbuflen = buflen;
    *snumPackets = xfer->numPackets;

    vector[0].base = sep;
    vector[0].length = sizeof(u8);
    vector[1].base = (u8*)sbuflen;
    vector[1].length = sizeof(u16);
    vector[2].base = snumPackets;
    vector[2].length = sizeof(u8);
    vector[3].base = (u8*)xfer->packets;
    vector[3].length = sizeof(u16) * xfer->numPackets;
    vector[4].base = xfer->buf;
    vector[4].length = buflen;

    DCFlushRange(sep, ROUNDUP(sizeof(u8)));
    DCFlushRange(sbuflen, ROUNDUP(sizeof(u16)));
    DCFlushRange(snumPackets, ROUNDUP(sizeof(u8)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 5);

    req->icb = cb;
    req->xfer = xfer;
    req->cbArg = cbArg;
    req->nclean = 4;
    req->clean[0] = sep;
    req->clean[1] = sbuflen;
    req->clean[2] = snumPackets;
    req->clean[3] = vector;

    rv = IOS_IoctlvAsync(fd, USB_IOCTL_ISOMSG, 3, 2, vector, _intrBlkCtrlIsoCb, req);
    if (rv < 0) {
        IOSFree(req);
        goto clean;
    }
    goto out;

clean:
    IOSFree(sep);
    IOSFree(sbuflen);
    IOSFree(snumPackets);
    IOSFree(vector);

out:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_DeviceInsertionNotifyAsync
 *
 * Calls cb when a device with the given vendor and product id is attached under path.
 */
IOSError IUSB_DeviceInsertionNotifyAsync(const char* path, u16 vid, u16 pid, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv;
    IOSFd fd;
    IOSIoVector* vector;
    u16* svid;
    u16* spid;
    iusb_ctxt* req;

    if (0 == path || 0 == vid || 0 == pid) {
        rv = IOS_ERROR_INVALID;
        goto done;
    }
    if ((fd = IOS_Open(path, 0)) < 0) {
        rv = fd;
        USB_ERR("Open(%s) failed\n", path);
        goto done;
    }

    vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 2);
    spid = IOSAlloc(ROUNDUP(sizeof(u16)));
    svid = IOSAlloc(ROUNDUP(sizeof(u16)));
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (0 == vector || 0 == spid || 0 == svid || 0 == req) {
        USB_ERR("getDeviceList: Not enough memory\n");
        rv = IOS_ERROR_FAIL_ALLOC;
        goto clean;
    }

    memset(req, 0, sizeof(iusb_ctxt));
    *svid = vid;
    *spid = pid;

    vector[0].base = (u8*)svid;
    vector[0].length = sizeof(u16);
    vector[1].base = (u8*)spid;
    vector[1].length = sizeof(u16);
    DCFlushRange(svid, ROUNDUP(sizeof(u16)));
    DCFlushRange(spid, ROUNDUP(sizeof(u16)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 2);
    req->cb = cb;
    req->cbArg = cbArg;
    req->nclean = 3;
    req->clean[0] = svid;
    req->clean[1] = spid;
    req->clean[2] = vector;

    rv = IOS_IoctlvAsync(fd, USB_IOCTL_DEVINSERTHOOK, 2, 0, vector, _intrBlkCtrlIsoCb, req);
    if (rv < 0) {
        goto clean;
    }
    IOS_Close(fd);
    goto done;

clean:
    IOSFree(svid);
    IOSFree(spid);
    IOSFree(vector);
    IOSFree(req);

done:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_DeviceClassInsertionNotifyAsync
 *
 * Calls cb when a device of the given class is attached under path.
 */
IOSError IUSB_DeviceClassInsertionNotifyAsync(const char* path, u8 devClass, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv;
    IOSFd fd;
    IOSIoVector* vector;
    u8* sdevClass;
    iusb_ctxt* req;

    if (0 == path || 0 == devClass) {
        OSReport("Invalid path or devClass in insertion notification call\n");
        rv = IOS_ERROR_INVALID;
        goto done;
    }
    if ((fd = IOS_Open(path, 0)) < 0) {
        rv = fd;
        OSReport("Open(%s) failed\n", path);
        goto done;
    }

    vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 2);
    sdevClass = IOSAlloc(ROUNDUP(sizeof(u8)));
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    *sdevClass = devClass;

    vector[0].base = sdevClass;
    vector[0].length = sizeof(u8);
    DCFlushRange(sdevClass, ROUNDUP(sizeof(u8)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 2);
    req->cb = cb;
    req->cbArg = cbArg;
    req->nclean = 2;
    req->clean[0] = sdevClass;
    req->clean[1] = vector;

    rv = IOS_IoctlvAsync(fd, USB_IOCTL_DEVCLASSCHANGE, 1, 0, vector, _intrBlkCtrlIsoCb, req);
    IOS_Close(fd);

done:
    return rv;
} // no copy in retail (dead-stripped)

/*
 * IUSB_RegisterInsertionNotifyWithIdAsync
 *
 * Calls cb when a device with the given vendor and product id is attached under path, reporting it by id.
 * No reference has this body: its signature and text are reconstructed from its three retail strings
 * (its __FUNCTION__, the allocation failure and the open failure) on the pattern of
 * IUSB_DeviceInsertionNotifyAsync (open item in docs/exceptions.md).
 */
IOSError IUSB_RegisterInsertionNotifyWithIdAsync(const char* path, u16 vid, u16 pid, USBCallbackFunc cb, void* cbArg)
{
    IOSError rv;
    IOSFd fd;
    IOSIoVector* vector;
    u16* svid;
    u16* spid;
    iusb_ctxt* req;

    vector = IOSAlloc(ROUNDUP(sizeof(IOSIoVector)) * 2);
    svid = IOSAlloc(ROUNDUP(sizeof(u16)));
    spid = IOSAlloc(ROUNDUP(sizeof(u16)));
    req = IOSAlloc(ROUNDUP(sizeof(*req)));
    if (0 == vector || 0 == svid || 0 == spid || 0 == req) {
        USB_ERR("%s: Not enough memory\n", __FUNCTION__);
        rv = IOS_ERROR_FAIL_ALLOC;
        goto clean;
    }

    if ((fd = IOS_Open(path, 0)) < 0) {
        USB_ERR("Failed to open %s: %d\n", path, fd);
        rv = fd;
        goto clean;
    }

    memset(req, 0, sizeof(iusb_ctxt));
    *svid = vid;
    *spid = pid;

    vector[0].base = (u8*)svid;
    vector[0].length = sizeof(u16);
    vector[1].base = (u8*)spid;
    vector[1].length = sizeof(u16);
    DCFlushRange(svid, ROUNDUP(sizeof(u16)));
    DCFlushRange(spid, ROUNDUP(sizeof(u16)));
    DCFlushRange(vector, ROUNDUP(sizeof(IOSIoVector)) * 2);
    req->cb = cb;
    req->cbArg = cbArg;
    req->nclean = 3;
    req->clean[0] = svid;
    req->clean[1] = spid;
    req->clean[2] = vector;

    rv = IOS_IoctlvAsync(fd, USB_IOCTL_DEVINSERTHOOK, 2, 0, vector, _intrBlkCtrlIsoCb, req);
    IOS_Close(fd);
    if (rv >= 0) {
        goto done;
    }

clean:
    IOSFree(svid);
    IOSFree(spid);
    IOSFree(vector);
    IOSFree(req);

done:
    return rv;
} // no copy in retail (dead-stripped)
