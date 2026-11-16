// Reference: zeldaret/tp libs/revolution/src/os/OSNet.c
// Parked (data): retail .data keeps the caller-name strings of the resource-manager helpers (and
// "NWC24ResumeScheduler", a function with no copy in retail). The compiler emits them only while the
// callerName argument reaches code generation (out-of-line helpers, or a statement reading it), not
// for these fully inlined helpers, whose release bodies ignore it; both references force the strings.
#include <revolution/os.h>
#include <revolution/ipc/ipcclt.h>
#include <revolution/sc.h>

#define NWC24_ERR_PERM (-1)
#define NWC24_ERR_INVALID (-3)
#define NWC24_ERR_FIRMWARE (-29)
#define NWC24_ERR_IOS (-42)

/* IPC buffers handed to IOS must be 32-byte aligned. */
#define IPC_ALIGN __attribute__((aligned(32)))

#define NWC24_SHUTDOWN_PRIORITY 110
#define NWC24_SHUTDOWN_RETRIES 5

static OSShutdownFunctionInfo ShutdownFuncInfo;

static s32 nwc24ShtFd = -1;
static int nwc24ShtRetryRest = 0;
static BOOL NWC24iIsRequestPending = FALSE;

static s32 CallbackAsyncIpc(s32 result, void* arg);

/*
 * Resource manager helpers
 *
 * Wrappers over the IOS calls that map their failures to NWC24 error codes.
 * Only ever inlined; the caller name is unused in this build.
 */

/* Opens path; IOS returns -6 while the firmware is still starting up. */
static inline NWC24Err NWC24iOpenResourceManager_(const char* callerName, const char* path, s32* fd, u32 flags) {
    if (!fd) {
        return NWC24_ERR_INVALID;
    }

    *fd = IOS_Open(path, flags);
    if (*fd < 0) {
        if (*fd == -6) {
            return NWC24_ERR_FIRMWARE;
        } else {
            return NWC24_ERR_IOS;
        }
    }

    return NWC24_OK;
}

/* Closes fd. */
static inline NWC24Err NWC24iCloseResourceManager_(const char* callerName, s32 fd) {
    int rt = IOS_Close(fd);
    if (rt < 0) {
        return NWC24_ERR_IOS;
    }

    return NWC24_OK;
}

/* Synchronous ioctl on fd. */
static inline NWC24Err NWC24iIoctlResourceManager_(const char* callerName, s32 fd, s32 cmd, void* input, u32 inputLen, void* output, u32 outputLen) {
    int rt = IOS_Ioctl(fd, cmd, input, inputLen, output, outputLen);
    if (rt < 0) {
        return NWC24_ERR_IOS;
    }

    return NWC24_OK;
}

/* Asynchronous ioctl on fd; CallbackAsyncIpc clears the pending flag when it completes. */
static inline NWC24Err NWC24iIoctlResourceManagerAsync_(const char* callerName, s32 fd, s32 cmd, void* input, u32 inputLen, void* output, u32 outputLen, void* cbArg) {
    int rt = IOS_IoctlAsync(fd, cmd, input, inputLen, output, outputLen, CallbackAsyncIpc, cbArg);
    if (rt < 0) {
        return NWC24_ERR_IOS;
    }

    NWC24iIsRequestPending = TRUE;
    return NWC24_OK;
}

/* TRUE while an asynchronous request is outstanding. */
static inline BOOL NWC24iIsAsyncRequestPending_(void) {
    return NWC24iIsRequestPending;
}

/* The NWC24 calls are unavailable from an exception handler (no current thread). */
static inline s32 CheckCallingStatus(const char* callerName) {
    if (OSGetCurrentThread() == 0) {
        return NWC24_ERR_PERM;
    }

    return 0;
}

/* Reads the real-time clock as seconds, once the system configuration is ready. */
static inline NWC24Err GetRTC(int* rtc) {
    OSTime time;
    u32 bias;
    u32 status;

    do {
        status = SCCheckStatus();
        if (status == 2) {
            return NWC24_ERR_PERM;
        }
    } while (status != 0);

    bias = SCGetCounterBias();
    time = OSGetTime();
    *rtc = (time / OS_TIMER_CLOCK) - bias;
    return NWC24_OK;
}

/*
 * __OSInitNet
 *
 * Registers the network shutdown function and suspends the WiiConnect24
 * scheduler, then (outside the IPL) sets the network clock from the RTC.
 */
void __OSInitNet(void) {
    NWC24Err cur_err;
    s32 res;
    OSIOSRev iosRev;

    __OSGetIOSRev(&iosRev);

    if (iosRev.major <= 4 || iosRev.major == 9) {
        return;
    }

    cur_err = NWC24iPrepareShutdown();

    if (cur_err != NWC24_OK) {
        if (cur_err < NWC24_OK) {
            OSReport("Failed to register network shutdown function. %d\n", cur_err);
        }

        res = NWC24SuspendScheduler();
        if (res < NWC24_OK) {
            OSReport("Failed to suspend the WiiConnect24 scheduler. %d\n", res);
        }
    }

    if (!__OSInIPL) {
        cur_err = NWC24iSynchronizeRtcCounter(0);

        if (cur_err != NWC24_OK) {
            OSReport("Failed to synchronize time with network resource managers. %d\n", cur_err);
        }
    }
} // 0x8021D710

/*
 * NWC24iPrepareShutdown
 *
 * Registers NWC24Shutdown_ and opens the kd request resource manager.
 * Returns 1 when everything succeeded.
 */
NWC24Err NWC24iPrepareShutdown(void) {
    NWC24Err rt = NWC24_OK;

    ShutdownFuncInfo.func = NWC24Shutdown_;
    ShutdownFuncInfo.priority = NWC24_SHUTDOWN_PRIORITY;
    OSRegisterShutdownFunction(&ShutdownFuncInfo);

    if (nwc24ShtFd < 0) {
        rt = NWC24iOpenResourceManager_("NWC24iPrepareShutdown", "/dev/net/kd/request", &nwc24ShtFd, 1);
    }

    nwc24ShtRetryRest = NWC24_SHUTDOWN_RETRIES;

    if (rt == NWC24_OK) {
        rt = 1;
    }

    return rt;
} // 0x8021D7D0

/*
 * NWC24iSynchronizeRtcCounter
 *
 * Reads the RTC and hands it to the kd time resource manager.
 */
NWC24Err NWC24iSynchronizeRtcCounter(BOOL notify) {
    NWC24Err rt;
    int rtc;

    rt = GetRTC(&rtc);
    if (rt != NWC24_OK) {
        return rt;
    }

    return NWC24iSetRtcCounter_(rtc, notify ? TRUE : FALSE);
} // 0x8021D890

/*
 * NWC24SuspendScheduler
 *
 * Asks the kd request resource manager to suspend the WiiConnect24 scheduler.
 */
NWC24Err NWC24SuspendScheduler(void) {
    static u8 susResult[0x20] IPC_ALIGN;
    NWC24Err rt = NWC24_OK;
    NWC24Err closeRt = NWC24_OK;
    s32 fd;

    rt = CheckCallingStatus("NWC24SuspendScheduler");
    if (rt < NWC24_OK) {
        return rt;
    }

    rt = NWC24iOpenResourceManager_("NWC24SuspendScheduler", "/dev/net/kd/request", &fd, 0);
    if (rt >= NWC24_OK) {
        rt = NWC24iIoctlResourceManager_("NWC24SuspendScheduler", fd, 1, NULL, 0, &susResult, sizeof(susResult));
        if (rt >= NWC24_OK) {
            rt = *(NWC24Err*)susResult;
        }

        closeRt = NWC24iCloseResourceManager_("NWC24SuspendScheduler", fd);
        if (closeRt < NWC24_OK) {
            rt = closeRt;
        }
    }

    return rt;
} // 0x8021D930

/*
 * NWC24iRequestShutdown
 *
 * Sends the shutdown event to the kd request resource manager; the reply
 * code lands in *result.
 */
NWC24Err NWC24iRequestShutdown(u32 event, s32* result) {
    static u32 shtBuffer[8] IPC_ALIGN;
    static u32 shtResult[8] IPC_ALIGN;
    NWC24Err ret = NWC24_OK;

    shtBuffer[0] = event;
    ret = NWC24iIoctlResourceManagerAsync_("NWC24iRequestShutdown", nwc24ShtFd, 0x28, shtBuffer, 0x20, shtResult, 0x20, result);
    return ret;
} // 0x8021DA30

/*
 * NWC24Shutdown_
 *
 * Shutdown function: sends the shutdown request, waits for the reply and
 * retries a few times before giving up.
 */
BOOL NWC24Shutdown_(BOOL final, u32 event) {
    static BOOL shuttingdown;
    static s32 result;

    if (final) {
        return TRUE;
    }

    if (shuttingdown) {
        if (NWC24iIsAsyncRequestPending_()) {
            return FALSE;
        }

        if (result >= 0) {
            return TRUE;
        }

        if (nwc24ShtRetryRest > 0) {
            shuttingdown = FALSE;
            nwc24ShtRetryRest--;
        } else {
            OSReport("NWC24Shutdown_: Give up!\n");
            return 1;
        }
    } else {
        if (NWC24iRequestShutdown(event, &result) >= NWC24_OK) {
            shuttingdown = TRUE;
        }
    }

    return FALSE;
} // 0x8021DAA0

static u32 nwc24TimeCommonBuffer[8] IPC_ALIGN;
static u32 nwc24TimeCommonResult[8] IPC_ALIGN;

/*
 * NWC24iSetRtcCounter_
 *
 * Sends the RTC counter to the kd time resource manager.
 */
NWC24Err NWC24iSetRtcCounter_(s32 rtc, BOOL notify) {
    NWC24Err rt = NWC24_OK;
    NWC24Err closeRt = NWC24_OK;
    s32 fd;

    rt = CheckCallingStatus("NWC24iSetRtcCounter_");
    if (rt < NWC24_OK) {
        return rt;
    }

    rt = NWC24iOpenResourceManager_("NWC24iSetRtcCounter_", "/dev/net/kd/time", &fd, 0);
    if (rt >= NWC24_OK) {
        nwc24TimeCommonBuffer[0] = rtc;
        nwc24TimeCommonBuffer[1] = notify;

        rt = NWC24iIoctlResourceManager_("NWC24iSetRtcCounter_", fd, 0x17, &nwc24TimeCommonBuffer, sizeof(nwc24TimeCommonBuffer), &nwc24TimeCommonResult, sizeof(nwc24TimeCommonResult));
        if (rt >= NWC24_OK) {
            rt = *(int*)nwc24TimeCommonResult;
        }

        closeRt = NWC24iCloseResourceManager_("NWC24iSetRtcCounter_", fd);
        if (rt >= NWC24_OK) {
            rt = closeRt;
        }
    }

    return rt;
} // 0x8021DB60

/*
 * CallbackAsyncIpc
 *
 * Completion callback of the asynchronous ioctl: stores the result in *arg
 * and clears the pending flag.
 */
static s32 CallbackAsyncIpc(s32 result, void* arg) {
    if (arg != 0) {
        *(s32*)arg = result;
    }

    NWC24iIsRequestPending = FALSE;
    return 0;
} // 0x8021DC90
