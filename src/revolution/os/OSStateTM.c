// Reference: zeldaret/tp libs/revolution/src/os/OSStateTM.c
#include <revolution/hardware.h>
#include <revolution/ipc.h>
#include <revolution/os.h>

/* STM (state manager) ioctl commands and event codes; names invented, values from the call sites. */
#define STM_IOCTL_EVENTHOOK_REGISTER 0x1000
#define STM_IOCTL_HOT_RESET 0x2001
#define STM_IOCTL_SHUTDOWN_TO_STANDBY 0x2003
#define STM_IOCTL_EVENTHOOK_UNREGISTER 0x3002
#define STM_IOCTL_VIDIM 0x5001

/* Fixed trailing words of the dimming request; meaning unknown (names invented). */
#define STM_VIDIM_MASK_HI 0xFFFFFFFF
#define STM_VIDIM_MASK_LO 0xFFFF0000

#define STM_EVENT_POWER 0x00000800
#define STM_EVENT_RESET 0x00020000

/* Bit of the PI interrupt-cause word that is clear while the reset button is held. */
#define PI_INTSR_RSTSWST 0x00010000

static u32 StmEhInBuf[8] __attribute__((aligned(32)));
static u32 StmEhOutBuf[8] __attribute__((aligned(32)));

static u32 StmImInBuf[8] __attribute__((aligned(32)));
static u32 StmImOutBuf[8] __attribute__((aligned(32)));

static u32 StmVdInBuf[8] __attribute__((aligned(32)));
static u32 StmVdOutBuf[8] __attribute__((aligned(32)));

static BOOL ResetDown = 0;
static int StmReady = 0;
static int StmImDesc = 0;
static int StmEhDesc = 0;
static int StmEhRegistered = 0;
static int StmVdInUse = 0;

static OSResetCallback ResetCallback;
static OSPowerCallback PowerCallback;

static s32 __OSStateEventHandler(s32 ret, void* arg);
static s32 __OSVIDimReplyHandler(s32 ret, void* arg);
static void __OSDefaultResetCallback(void);
static void __OSDefaultPowerCallback(void);

static BOOL __OSGetResetButtonStateRaw(void);
static void __OSRegisterStateEvent(void);
static int AccessVIDimRegs(void);
static void LockUp(void);

/*
 * OSSetResetCallback
 *
 * Installs the function run when the reset button is pressed (NULL restores
 * the default). Returns the previous callback, or NULL if it was the default.
 */
OSResetCallback OSSetResetCallback(OSResetCallback callback) {
    BOOL enabled;
    OSResetCallback prevCallback;

    enabled = OSDisableInterrupts();
    prevCallback = ResetCallback;

    if (callback) {
        ResetCallback = callback;
    } else {
        ResetCallback = __OSDefaultResetCallback;
    }

    if (!StmEhRegistered) {
        __OSRegisterStateEvent();
    }

    OSRestoreInterrupts(enabled);

    if (prevCallback == __OSDefaultResetCallback) {
        return NULL;
    } else {
        return prevCallback;
    }
} // 0x8021C6C0

/*
 * OSSetPowerCallback
 *
 * Installs the function run when the power button is pressed (NULL restores
 * the default). Returns the previous callback, or NULL if it was the default.
 */
OSPowerCallback OSSetPowerCallback(OSPowerCallback callback) {
    BOOL enabled;
    OSPowerCallback prevCallback;

    enabled = OSDisableInterrupts();
    prevCallback = PowerCallback;

    if (callback) {
        PowerCallback = callback;
    } else {
        PowerCallback = __OSDefaultPowerCallback;
    }

    if (!StmEhRegistered) {
        __OSRegisterStateEvent();
    }

    OSRestoreInterrupts(enabled);

    if (prevCallback == __OSDefaultPowerCallback) {
        return NULL;
    } else {
        return prevCallback;
    }
} // 0x8021C7B0

/*
 * __OSInitSTM
 *
 * Opens the STM immediate and event hook drivers and hooks the state events.
 * Returns 0 if the firmware has no STM.
 */
int __OSInitSTM(void) {
    PowerCallback = __OSDefaultPowerCallback;
    ResetCallback = __OSDefaultResetCallback;
    ResetDown = 0;

    if (StmReady) {
        return 1;
    }

    StmVdInUse = 0;

    StmImDesc = IOS_Open("/dev/stm/immediate", 0);
    if (StmImDesc < 0) {
        StmReady = 0;
        return 0;
    }

    StmEhDesc = IOS_Open("/dev/stm/eventhook", 0);
    if (StmEhDesc < 0) {
        StmReady = 0;
        return 0;
    }

    __OSRegisterStateEvent();
    StmReady = 1;
    return 1;
} // 0x8021C8A0

/*
 * __OSShutdownToSBY
 *
 * Asks the STM to put the console in standby, then locks up.
 */
void __OSShutdownToSBY(void) {
    __VIRegs[1] = 0;

    if (!StmReady) {
        OSPanic(__FILE__, 348, "Error: The firmware doesn't support shutdown feature.\n");
    }

    StmImInBuf[0] = 0;
    IOS_Ioctl(StmImDesc, STM_IOCTL_SHUTDOWN_TO_STANDBY, StmImInBuf, sizeof(StmImInBuf), StmImOutBuf, sizeof(StmImOutBuf));
    LockUp();
} // 0x8021C9C0

/*
 * __OSHotReset
 *
 * Asks the STM for a hot reset, then locks up.
 */
void __OSHotReset(void) {
    __VIRegs[1] = 0;

    if (!StmReady) {
        OSPanic(__FILE__, 412, "Error: The firmware doesn't support reboot feature.\n");
    }

    IOS_Ioctl(StmImDesc, STM_IOCTL_HOT_RESET, StmImInBuf, sizeof(StmImInBuf), StmImOutBuf, sizeof(StmImOutBuf));
    LockUp();
} // 0x8021CA40

/* Returns TRUE while the reset button is held. */
static BOOL __OSGetResetButtonStateRaw(void) {
    u32 reg = __PIRegs[0];

    if (!(reg & PI_INTSR_RSTSWST)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/*
 * __OSSetVIForceDimming
 *
 * Starts the STM's video dimming with the given enable flag and shifts.
 * Returns 1 if the request was queued, 0 if one is already in flight, or a
 * negative IOS error.
 */
int __OSSetVIForceDimming(BOOL isEnabled, u32 yShift, u32 xShift) {
    BOOL en;
    int res;

    if (!StmReady) {
        return -10;
    }

    en = OSDisableInterrupts();

    if (StmVdInUse) {
        OSRestoreInterrupts(en);
        return 0;
    }

    StmVdInUse = 1;
    OSRestoreInterrupts(en);

    StmVdInBuf[0] = (xShift | yShift << 3) | isEnabled << 7;
    StmVdInBuf[1] = 0;
    StmVdInBuf[2] = 0;
    StmVdInBuf[3] = 0;
    StmVdInBuf[4] = 0;
    StmVdInBuf[5] = STM_VIDIM_MASK_HI;
    StmVdInBuf[6] = STM_VIDIM_MASK_LO;
    StmVdInBuf[7] = 0;

    res = AccessVIDimRegs();
    return res;
} // 0x8021CAC0

/*
 * __OSUnRegisterStateEvent
 *
 * Removes the state event hook. Returns 0 on success.
 */
s32 __OSUnRegisterStateEvent(void) {
    s32 ret;

    if (StmEhRegistered == 0) {
        return 0;
    }

    if (StmReady == 0) {
        return -6;
    }

    ret = IOS_Ioctl(StmImDesc, STM_IOCTL_EVENTHOOK_UNREGISTER, StmImInBuf, sizeof(StmImInBuf), StmImOutBuf,
                    sizeof(StmImOutBuf));

    if (ret == 0) {
        StmEhRegistered = 0;
    }

    return ret;
} // 0x8021CBC0

/* Sends the prepared dimming request to the STM. Returns 1 if queued, else the IOS error. */
static int AccessVIDimRegs(void) {
    int res;

    res = IOS_IoctlAsync(StmImDesc, STM_IOCTL_VIDIM, StmVdInBuf, sizeof(StmVdInBuf), StmVdOutBuf, sizeof(StmVdOutBuf),
                         __OSVIDimReplyHandler, 0);
    switch (res) {
    default:
        return res;
    case 0:
        return 1;
    }
}

/*
 * __OSVIDimReplyHandler
 *
 * Completion callback of the VI dimming request.
 */
static s32 __OSVIDimReplyHandler(s32 ret, void* arg) {
    StmVdInUse = 0;
    return 0;
} // 0x8021CC40

/* Arms the STM event hook; the reply arrives in __OSStateEventHandler. */
static void __OSRegisterStateEvent(void) {
    int err;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    err = IOS_IoctlAsync(StmEhDesc, STM_IOCTL_EVENTHOOK_REGISTER, StmEhInBuf, sizeof(StmEhInBuf), StmEhOutBuf,
                         sizeof(StmEhOutBuf), __OSStateEventHandler, 0);

    if (err == 0) {
        StmEhRegistered = 1;
    } else {
        StmEhRegistered = 0;
    }

    OSRestoreInterrupts(enabled);
}

static void __OSDefaultResetCallback(void) {
} // 0x8021CC50

static void __OSDefaultPowerCallback(void) {
} // 0x8021CC60

/*
 * __OSStateEventHandler
 *
 * Completion callback of the event hook: runs the reset or power callback
 * and re-arms the hook.
 */
static s32 __OSStateEventHandler(s32 ret, void* arg) {
    BOOL enabled;
    OSResetCallback resetCb;
    OSPowerCallback powerCb;

    if (ret != 0) {
        OSPanic(__FILE__, 820, "Error on STM state event handler\n");
    }

    StmEhRegistered = 0;

    if (StmEhOutBuf[0] == STM_EVENT_RESET) {
        if (__OSGetResetButtonStateRaw()) {
            enabled = OSDisableInterrupts();
            ResetDown = TRUE;
            resetCb = ResetCallback;
            ResetCallback = __OSDefaultResetCallback;
            resetCb();
            OSRestoreInterrupts(enabled);
            VIResetDimmingCount();
        }

        __OSRegisterStateEvent();
    }

    if (StmEhOutBuf[0] == STM_EVENT_POWER) {
        enabled = OSDisableInterrupts();
        powerCb = PowerCallback;
        PowerCallback = __OSDefaultPowerCallback;
        powerCb();
        OSRestoreInterrupts(enabled);
    }

    return 0;
} // 0x8021CC70

/* Flushes the instruction cache with interrupts off and spins forever. */
static void LockUp(void) {
    OSDisableInterrupts();
    ICFlashInvalidate();

    while (TRUE) {
    }
}
