// Reference: zeldaret/tp libs/revolution/src/exi/EXIBios.c
#include <revolution/os.h>
#include <revolution/hardware.h>
#include <revolution/exi.h>
#include <string.h>

/* Register layout: five words per channel. */
#define EXI_REG_MAX 5
#define EXI_REG_CPR 0
#define EXI_REG_MAR 1
#define EXI_REG_LENGTH 2
#define EXI_REG_CR 3
#define EXI_REG_DATA 4
#define REG(chan, idx) (__EXIRegs[(chan) * EXI_REG_MAX + (idx)])

#define STATE_IDLE     0
#define STATE_DMA      1
#define STATE_IMM      2
#define STATE_BUSY     3
#define STATE_SELECTED 4
#define STATE_ATTACHED 8
#define STATE_LOCKED   16

#define EXI_0LENGTH_EXILENGTH_MASK 0xFFFFFFE0

#define BUILD_DATE "Feb 27 2009"
#define BUILD_TIME "10:02:03"

const char* __EXIVersion = "<< RVL_SDK - EXI \trelease build: " BUILD_DATE " " BUILD_TIME " (0x4302_145) >>";

static EXIControl Ecb[3];
static u32 IDSerialPort1;

static int __EXIProbe(s32 chan);
static int __EXIAttach(s32 chan, EXICallback extCallback);

/*
 * SetExiInterruptMask
 */
static void SetExiInterruptMask(s32 chan, EXIControl* exi) {
    EXIControl* exi2;
    exi2 = &Ecb[2];

    switch (chan) {
    case 0:
        if ((exi->exiCallback == 0 && exi2->exiCallback == 0) || (exi->state & STATE_LOCKED)) {
            __OSMaskInterrupts(OS_INTERRUPTMASK_EXI_0_EXI | OS_INTERRUPTMASK_EXI_2_EXI);
        } else {
            __OSUnmaskInterrupts(OS_INTERRUPTMASK_EXI_0_EXI | OS_INTERRUPTMASK_EXI_2_EXI);
        }
        break;
    case 1:
        if (exi->exiCallback == 0 || (exi->state & STATE_LOCKED)) {
            __OSMaskInterrupts(OS_INTERRUPTMASK_EXI_1_EXI);
        } else {
            __OSUnmaskInterrupts(OS_INTERRUPTMASK_EXI_1_EXI);
        }
        break;
    case 2:
        if (__OSGetInterruptHandler(__OS_INTERRUPT_PI_DEBUG) == 0 || (exi->state & STATE_LOCKED)) {
            __OSMaskInterrupts(OS_INTERRUPTMASK_PI_DEBUG);
        } else {
            __OSUnmaskInterrupts(OS_INTERRUPTMASK_PI_DEBUG);
        }
        break;
    }
} // 0x8020D780

/*
 * CompleteTransfer
 */
static void CompleteTransfer(s32 chan) {
    EXIControl* exi;
    u8* buf;
    u32 data;
    int i;
    int len;

    exi = &Ecb[chan];

    if (exi->state & STATE_BUSY) {
        if (exi->state & STATE_IMM) {
            if (exi->immLen != 0) {
                len = exi->immLen;
                buf = exi->immBuf;
                data = REG(chan, EXI_REG_DATA);
                for (i = 0; i < len; i++) {
                    *buf++ = data >> ((3 - i) * 8);
                }
            }
        }
        exi->state &= ~STATE_BUSY;
    }
}

BOOL EXIImm(s32 chan, void* buf, s32 len, u32 type, EXICallback callback) {
    EXIControl* exi;
    BOOL enabled;

    exi = &Ecb[chan];
    enabled = OSDisableInterrupts();

    if ((exi->state & STATE_BUSY) || !(exi->state & STATE_SELECTED)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->tcCallback = callback;
    if (exi->tcCallback) {
        EXIClearInterrupts(chan, 0, 1, 0);
        __OSUnmaskInterrupts(OS_INTERRUPTMASK_EXI_0_TC >> (chan * 3));
    }

    exi->state |= STATE_IMM;
    if (type != EXI_READ) {
        u32 data;
        int i;

        data = 0;
        for (i = 0; i < len; i++) {
            data |= ((u8*)buf)[i] << ((3 - i) * 8);
        }
        REG(chan, EXI_REG_DATA) = data;
    }

    exi->immBuf = buf;
    exi->immLen = (type != EXI_WRITE) ? len : 0;
    REG(chan, EXI_REG_CR) = (type << 2) | 1 | ((len - 1) << 4);
    OSRestoreInterrupts(enabled);
    return TRUE;
} // 0x8020D870

BOOL EXIImmEx(s32 chan, void* buf, s32 len, u32 mode) {
    s32 xLen;

    while (len) {
        xLen = (len < 4) ? len : 4;
        if (EXIImm(chan, buf, xLen, mode, 0) == 0) {
            return FALSE;
        }
        if (EXISync(chan) == 0) {
            return FALSE;
        }
        ((u8*)buf) += xLen;
        len -= xLen;
    }

    return TRUE;
} // 0x8020DAF0

BOOL EXIDma(s32 chan, void* buf, s32 len, u32 type, EXICallback callback) {
    EXIControl* exi;
    BOOL enabled;

    exi = &Ecb[chan];

    enabled = OSDisableInterrupts();
    if ((exi->state & STATE_BUSY) || !(exi->state & STATE_SELECTED)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    exi->tcCallback = callback;
    if ((u32)exi->tcCallback) {
        EXIClearInterrupts(chan, 0, 1, 0);
        __OSUnmaskInterrupts(OS_INTERRUPTMASK_EXI_0_TC >> (chan * 3));
    }

    exi->state |= STATE_DMA;
    REG(chan, EXI_REG_MAR) = (u32)buf & EXI_0LENGTH_EXILENGTH_MASK;
    REG(chan, EXI_REG_LENGTH) = len;
    REG(chan, EXI_REG_CR) = (type * 4) | 3;

    OSRestoreInterrupts(enabled);
    return TRUE;
} // 0x8020DBA0

BOOL EXISync(s32 chan) {
    EXIControl* exi;
    int rc;
    BOOL enabled;

    exi = &Ecb[chan];
    rc = 0;

    while ((exi->state & STATE_SELECTED)) {
        if (!(REG(chan, EXI_REG_CR) & 1)) {
            enabled = OSDisableInterrupts();
            if (exi->state & STATE_SELECTED) {
                CompleteTransfer(chan);
                if (__OSGetDIConfig() != 0xFF || (OSGetConsoleType() & 0xf0000000) == 0x20000000 || exi->immLen != 4 || (REG(chan, EXI_REG_CPR) & 0x70) || (REG(chan, EXI_REG_DATA) != 0x01010000 && REG(chan, EXI_REG_DATA) != 0x05070000 && REG(chan, EXI_REG_DATA) != 0x04220001) || __OSDeviceCode == 0x8200) {
                    rc = 1;
                }
            }
            OSRestoreInterrupts(enabled);
            break;
        }
    }

    return rc;
} // 0x8020DCA0

u32 EXIClearInterrupts(s32 chan, int exi, int tc, int ext) {
    u32 cpr;
    u32 prev;

    cpr = prev = REG(chan, EXI_REG_CPR);
    prev &= 0x7F5;

    if (exi != 0) {
        prev |= 2;
    }

    if (tc != 0) {
        prev |= 8;
    }

    if (ext != 0) {
        prev |= 0x800;
    }

    REG(chan, EXI_REG_CPR) = prev;
    return cpr;
}

EXICallback EXISetExiCallback(s32 chan, EXICallback exiCallback) {
    EXIControl* exi;
    EXICallback prev;
    BOOL enabled;

    exi = &Ecb[chan];
    enabled = OSDisableInterrupts();

    prev = exi->exiCallback;
    exi->exiCallback = exiCallback;
    if (chan != 2) {
        SetExiInterruptMask(chan, exi);
    } else {
        SetExiInterruptMask(0, &Ecb[0]);
    }

    OSRestoreInterrupts(enabled);
    return prev;
} // 0x8020DF20

void EXIProbeReset(void) {
    ExiProbeStartTime[0] = ExiProbeStartTime[1] = 0;
    Ecb[0].idTime = Ecb[1].idTime = 0;
    __EXIProbe(0);
    __EXIProbe(1);
}

static int __EXIProbe(s32 chan) {
    EXIControl* exi;
    BOOL enabled;
    int rc;
    u32 cpr;
    s32 t;

    exi = &Ecb[chan];
    if (chan == 2) {
        return 1;
    }

    rc = 1;
    enabled = OSDisableInterrupts();
    cpr = REG(chan, EXI_REG_CPR);

    if (!(exi->state & STATE_ATTACHED)) {
        if (cpr & 0x800) {
            EXIClearInterrupts(chan, 0, 0, 1);
            ExiProbeStartTime[chan] = exi->idTime = 0;
        }

        if (cpr & 0x1000) {
            t = ((s32)(OSTicksToMilliseconds(OSGetTime()) / 100) + 1);

            if (ExiProbeStartTime[chan] == 0) {
                ExiProbeStartTime[chan] = t;
            }

            if (t - ExiProbeStartTime[chan] < 3) {
                rc = 0;
            }
        } else {
            ExiProbeStartTime[chan] = exi->idTime = 0;
            rc = 0;
        }
    } else if (!(cpr & 0x1000) || (cpr & 0x800)) {
        ExiProbeStartTime[chan] = exi->idTime = 0;
        rc = 0;
    }

    OSRestoreInterrupts(enabled);
    return rc;
} // 0x8020DFB0

BOOL EXIProbe(s32 chan) {
    EXIControl* exi = &Ecb[chan];
    int rc;
    u32 id;

    rc = __EXIProbe(chan);
    if (rc && !exi->idTime) {
        rc = EXIGetID(chan, 0, &id) ? 1 : 0;
    }

    return rc;
}

static int __EXIAttach(s32 chan, EXICallback extCallback) {
    EXIControl* exi;
    BOOL enabled;

    exi = &Ecb[chan];
    enabled = OSDisableInterrupts();

    if ((exi->state & STATE_ATTACHED) || !__EXIProbe(chan)) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    EXIClearInterrupts(chan, 1, 0, 0);
    exi->extCallback = extCallback;
    __OSUnmaskInterrupts(OS_INTERRUPTMASK_EXI_0_EXT >> (chan * 3));
    exi->state |= STATE_ATTACHED;

    OSRestoreInterrupts(enabled);
    return 1;
}

BOOL EXIAttach(s32 chan, EXICallback extCallback) {
    EXIControl* exi;
    BOOL enabled;
    int rc;

    exi = &Ecb[chan];

    EXIProbe(chan);
    enabled = OSDisableInterrupts();
    if (exi->idTime == 0) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    rc = __EXIAttach(chan, extCallback);
    OSRestoreInterrupts(enabled);
    return rc;
} // 0x8020E130

BOOL EXIDetach(s32 chan) {
    EXIControl* exi;
    BOOL enabled;

    exi = &Ecb[chan];
    enabled = OSDisableInterrupts();

    if (!(exi->state & STATE_ATTACHED)) {
        OSRestoreInterrupts(enabled);
        return 1;
    }

    if ((exi->state & STATE_LOCKED) && (exi->dev == 0)) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    exi->state &= ~STATE_ATTACHED;
    __OSMaskInterrupts((OS_INTERRUPTMASK_EXI_0_EXT | OS_INTERRUPTMASK_EXI_0_EXI) >> (chan * 3));

    OSRestoreInterrupts(enabled);
    return 1;
} // 0x8020E250

BOOL EXISelect(s32 chan, u32 dev, u32 freq) {
    EXIControl* exi;
    u32 cpr;
    BOOL enabled;

    exi = &Ecb[chan];

    enabled = OSDisableInterrupts();
    if ((exi->state & STATE_SELECTED) || ((chan != 2) && (((dev == 0) && !(exi->state & STATE_ATTACHED) && (__EXIProbe(chan) == 0)) || !(exi->state & STATE_LOCKED) || (exi->dev != dev)))) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    exi->state |= STATE_SELECTED;
    cpr = REG(chan, EXI_REG_CPR);
    cpr &= 0x405;
    cpr |= (((1 << dev) << 7) | (freq * 0x10));
    REG(chan, EXI_REG_CPR) = cpr;

    if (exi->state & STATE_ATTACHED) {
        switch (chan) {
        case 0:
            __OSMaskInterrupts(OS_INTERRUPTMASK_EXI_0_EXT);
            break;
        case 1:
            __OSMaskInterrupts(OS_INTERRUPTMASK_EXI_1_EXT);
            break;
        }
    }

    OSRestoreInterrupts(enabled);
    return 1;
} // 0x8020E310

BOOL EXIDeselect(s32 chan) {
    EXIControl* exi;
    u32 cpr;
    BOOL enabled;

    exi = &Ecb[chan];
    enabled = OSDisableInterrupts();

    if (!(exi->state & STATE_SELECTED)) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    exi->state &= ~STATE_SELECTED;
    cpr = REG(chan, EXI_REG_CPR);
    REG(chan, EXI_REG_CPR) = cpr & 0x405;

    if (exi->state & STATE_ATTACHED) {
        switch (chan) {
        case 0:
            __OSUnmaskInterrupts(OS_INTERRUPTMASK_EXI_0_EXT);
            break;
        case 1:
            __OSUnmaskInterrupts(OS_INTERRUPTMASK_EXI_1_EXT);
            break;
        }
    }

    OSRestoreInterrupts(enabled);

    if ((chan != 2) && (cpr & 0x80)) {
        return __EXIProbe(chan) ? TRUE : FALSE;
    }

    return 1;
} // 0x8020E440

static void EXIIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    s32 chan;
    EXIControl* exi;
    EXICallback callback;

    chan = (interrupt - __OS_INTERRUPT_EXI_0_EXI) / 3;

    exi = &Ecb[chan];
    EXIClearInterrupts(chan, 1, 0, 0);

    callback = exi->exiCallback;
    if (callback) {
        OSContext exceptionContext;

        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        callback(chan, context);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
} // 0x8020E540

static void TCIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    s32 chan;
    EXIControl* exi;
    EXICallback callback;

    chan = (interrupt - __OS_INTERRUPT_EXI_0_TC) / 3;

    exi = &Ecb[chan];
    __OSMaskInterrupts(OSInterruptMaskOf(interrupt));
    EXIClearInterrupts(chan, 0, 1, 0);

    callback = exi->tcCallback;
    if (callback) {
        OSContext exceptionContext;

        exi->tcCallback = NULL;
        CompleteTransfer(chan);

        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        callback(chan, context);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
} // 0x8020E600

static void EXTIntrruptHandler(__OSInterrupt interrupt, OSContext* context) {
    s32 chan;
    EXIControl* exi;
    EXICallback callback;

    chan = (interrupt - __OS_INTERRUPT_EXI_0_EXT) / 3;

    __OSMaskInterrupts((OS_INTERRUPTMASK_EXI_0_EXT | OS_INTERRUPTMASK_EXI_0_EXI) >> (chan * 3));
    exi = &Ecb[chan];
    callback = exi->extCallback;
    exi->state &= ~STATE_ATTACHED;

    if (callback) {
        OSContext exceptionContext;

        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        exi->extCallback = NULL;
        callback(chan, context);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
} // 0x8020E820

void EXIInit(void) {
    u32 id;

    while (((REG(0, EXI_REG_CR) & 1) == 1) || ((REG(1, EXI_REG_CR) & 1) == 1) || ((REG(2, EXI_REG_CR) & 1) == 1)) {}

    __OSMaskInterrupts(OS_INTERRUPTMASK_EXI);
    REG(0, EXI_REG_CPR) = 0;
    REG(1, EXI_REG_CPR) = 0;
    REG(2, EXI_REG_CPR) = 0;
    REG(0, EXI_REG_CPR) = 0x2000;
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_0_EXI, EXIIntrruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_0_TC, TCIntrruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_0_EXT, EXTIntrruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_1_EXI, EXIIntrruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_1_TC, TCIntrruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_1_EXT, EXTIntrruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_2_EXI, EXIIntrruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_EXI_2_TC, TCIntrruptHandler);

    EXIGetID(0, 2, &IDSerialPort1);

    if (__OSInIPL) {
        EXIProbeReset();
    } else if (EXIGetID(0, 0, &id) && id == 0x7010000) {
        __OSEnableBarnacle(1, 0);
    } else if (EXIGetID(1, 0, &id) && id == 0x7010000) {
        __OSEnableBarnacle(0, 2);
    }

    OSRegisterVersion(__EXIVersion);
} // 0x8020E8F0

BOOL EXILock(s32 chan, u32 dev, EXICallback unlockedCallback) {
    EXIControl* exi;
    BOOL enabled;
    int i;

    exi = &Ecb[chan];
    enabled = OSDisableInterrupts();

    if (exi->state & STATE_LOCKED) {
        if (unlockedCallback) {
            for (i = 0; i < exi->items; i++) {
                if (exi->queue[i].dev == dev) {
                    OSRestoreInterrupts(enabled);
                    return 0;
                }
            }
            exi->queue[exi->items].callback = unlockedCallback;
            exi->queue[exi->items].dev = dev;
            exi->items++;
        }
        OSRestoreInterrupts(enabled);
        return 0;
    }

    exi->state |= STATE_LOCKED;
    exi->dev = dev;
    SetExiInterruptMask(chan, exi);
    OSRestoreInterrupts(enabled);
    return 1;
} // 0x8020EAC0

BOOL EXIUnlock(s32 chan) {
    EXIControl* exi;
    BOOL enabled;
    EXICallback unlockedCallback;

    exi = &Ecb[chan];
    enabled = OSDisableInterrupts();

    if (!(exi->state & STATE_LOCKED)) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    exi->state &= ~STATE_LOCKED;
    SetExiInterruptMask(chan, exi);
    if (exi->items > 0) {
        unlockedCallback = exi->queue[0].callback;
        if (--exi->items > 0) {
            memmove(&exi->queue[0], &exi->queue[1], exi->items * 8);
        }
        unlockedCallback(chan, 0);
    }

    OSRestoreInterrupts(enabled);
    return 1;
} // 0x8020EBC0

static void UnlockedHandler(s32 chan, OSContext* context) {
    u32 id;
    EXIGetID(chan, 0, &id);
} // 0x8020ECA0

s32 EXIGetID(s32 chan, u32 dev, u32* id) {
    EXIControl* exi = &Ecb[chan];
    int err;
    u32 cmd;
    s32 startTime;
    BOOL enabled;

    if (chan == 0 && dev == 2 && IDSerialPort1 != 0) {
        *id = IDSerialPort1;
        return 1;
    }

    if ((chan < 2) && (dev == 0)) {
        if ((__EXIProbe(chan) == 0)) {
            return 0;
        }

        if (exi->idTime == ExiProbeStartTime[chan]) {
            *id = exi->id;
            return exi->idTime;
        }

        if (!__EXIAttach(chan, NULL)) {
            return 0;
        }

        startTime = ExiProbeStartTime[chan];
    }

    enabled = OSDisableInterrupts();

    err = !EXILock(chan, dev, (chan < 2 && dev == 0) ? &UnlockedHandler : NULL);
    if (err == 0) {
        err = !EXISelect(chan, dev, 0);
        if (err == 0) {
            cmd = 0;
            err |= !EXIImm(chan, &cmd, 2, 1, 0);
            err |= !EXISync(chan);
            err |= !EXIImm(chan, id, 4, 0, 0);
            err |= !EXISync(chan);
            err |= !EXIDeselect(chan);
        }

        EXIUnlock(chan);
    }

    OSRestoreInterrupts(enabled);

    if ((chan < 2) && (dev == 0)) {
        EXIDetach(chan);
        enabled = OSDisableInterrupts();
        err |= ExiProbeStartTime[chan] != startTime;

        if (!err) {
            exi->id = *id;
            exi->idTime = startTime;
        }

        OSRestoreInterrupts(enabled);
        return err ? 0 : exi->idTime;
    }

    return err ? 0 : 1;
} // 0x8020ECD0
