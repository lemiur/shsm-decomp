// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/wpad/R4QK01/WPAD.c
// Reference: zeldaret/tp libs/revolution/src/wpad/WPAD.c, doldecomp/ogws src/revolution/WPAD/WPAD.c
//
// Retail is the Jun 2009 SDK (0x4302_145): the control blocks, the command queue and several callbacks differ from
// the references.
//
// WPADResetAutoSleepTimeCount is the one function whose name no evidence gives.
#include <revolution/wpad/__wpad.h>
#include <revolution/sc.h>
#include <revolution/wud.h>
#include <revolution/vi/__vi.h>
#include <string.h>

#include <revolution/hardware.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

// Library banner embedded in the retail binary.
const char* __WPADVersion = "<< RVL_SDK - WPAD 	release build: Jun 22 2009 18:33:21 (0x4302_145) >>";

// Control blocks, 32-byte aligned as in retail.
WPADControlBlock _wpd[WPAD_MAX_CONTROLLERS];
WPADControlBlock* _wpdcb[WPAD_MAX_CONTROLLERS];

// Stack of the manager (WPADiManageHandler), which runs from the alarm handler.
u8 __WPADiManageHandlerStack[0x1000] __attribute__((aligned(32)));

s8 _dev_handle_index[WUD_MAX_DEV_ENTRY];
OSAlarm _managerAlarm;

// Game identification handed to the extension libraries.
u8 _gametype;
const char* _gamecode;

// Settings read from the system configuration (SC) at WPADInit.
u8 _sleepTime;
u8 _dpdSensitivity;
u8 _sensorBarPos;
BOOL _rumble;
u8 _speakerVolume;
static u8 _scSetting;

static u8 _shutdown;
static s8 _afhChannel;
u8 _chan_active_state[WPAD_MAX_CONTROLLERS];
static BOOL _initialized;

// Set by KPAD: the sampling and connect callbacks belong to it, and WPAD warns when a game replaces them.
static BOOL _callbackByKPAD;

// Extension families the application enabled, and their initialisers: the extension libraries override these weak
// definitions when they are linked.
DECL_WEAK BOOL _enabledDRM;
DECL_WEAK BOOL _enabledGTR;
DECL_WEAK BOOL _enabledTRN;
DECL_WEAK BOOL _enabledVSM;
DECL_WEAK void (*_wpadDRMInit)(void);
DECL_WEAK void (*_wpadGTRInit)(void);
DECL_WEAK void (*_wpadTRNInit)(void);
DECL_WEAK void (*_wpadVSMInit)(void);

static int _recFlag = -1;
static int _recCnt;
static BOOL _startup;
static u8 _rumbleCnt[WPAD_MAX_CONTROLLERS];
static u8 _extCnt[WPAD_MAX_CONTROLLERS];
static u16 _afhCnt;
static u8 _checkCnt;
static u16 _senseCnt;
static u8 _regShutdown;

// Samples that differ this often in a row count as a real change; as many equal samples let the counter decay.
static u16 _wpad_diff_count_threshold[WPAD_MAX_NZFILTERS] = {6, 4, 6};
static u16 _wpad_hyst_count_threshold[WPAD_MAX_NZFILTERS] = {30, 30, 30};


static void WPADiConnCallback(WUDDevInfo* devInfo, u8 open);

// Allocator installed while the library is being torn down for a fatal error: it never hands out memory.
static void* noAlloc(u32 size) {
    return NULL;
}

static BOOL noFree(void* block) {
    return FALSE;
}

BOOL WPADIsEnabledVSM(void) {
    return _enabledVSM;
} // 0x8027D610

BOOL WPADIsEnabledTRN(void) {
    return _enabledTRN;
} // 0x8027D620

BOOL WPADIsEnabledGTR(void) {
    return _enabledGTR;
} // 0x8027D630

BOOL WPADIsEnabledDRM(void) {
    return _enabledDRM;
} // 0x8027D640

// Balance Board library hooks: weak, so the Balance Board library overrides them when it is linked.
DECL_WEAK s32 WBCSetupCalibration(void) {
    return WPAD_ERR_OK;
} // 0x8027D650

DECL_WEAK s32 WBCGetCalibrationStatus(void) {
    return 0;
} // 0x8027D660

DECL_WEAK s32 WBCGetBatteryLevel(u8 chan) {
    return 0;
} // 0x8027D670

// Balance Board hook defaults, the first functions of the unit.
s32 WBCReadDummy(WPADBLStatus* bl, f64* cells, s32 count) {
    DEBUGPrint("WBCReadDummy\n");
    return -1;
} // 0x8027D680

// Default Balance Board zero-point hook: reports "no controller".
s32 WBCSetZEROPointDummy(f64* zero_point, s32 count) {
    DEBUGPrint("WBCSetZEROPointDummy\n");
    return -1;
} // 0x8027D6B0

// Default Balance Board weight-correction hook: reports "no controller".
s32 WBCGetTGCWeightDummy(f64 weight, f64* tgc_weight, WPADBLStatus* bl) {
    DEBUGPrint("WBCGetTGCWeightDummy\n");
    return -1;
} // 0x8027D6E0

// Stops the Bluetooth stack for good: no more connections, no more input.
static inline void __WPADShutdown(void) {
    BOOL enabled = OSDisableInterrupts();
    int chan;

    if (_shutdown) {
        OSRestoreInterrupts(enabled);
        return;
    }
    _shutdown = TRUE;
    WUDSetVisibility(FALSE, FALSE);
    for (chan = 0; chan < WPAD_MAX_CONTROLLERS; chan++) {
        WUDSetDeviceHistory(chan, NULL);
    }
    OSCancelAlarm(&_managerAlarm);
    WUDSetHidRecvCallback(NULL);
    WUDShutdown(FALSE);
    OSRestoreInterrupts(enabled);
}

// Asks the manager alarm to restart the stack after a delay; saveSimpleDevs keeps the simple-sync devices.
static inline void __WPADReconnect(BOOL saveSimpleDevs) {
    BOOL enabled = OSDisableInterrupts();

    if (!_startup) {
        OSRestoreInterrupts(enabled);
        return;
    }
    if (_shutdown) {
        OSRestoreInterrupts(enabled);
        return;
    }
    _shutdown = TRUE;
    _recFlag = saveSimpleDevs ? TRUE : FALSE;
    DEBUGPrint("Wait for %d ms until start reconnect!\n", _recCnt);
    OSRestoreInterrupts(enabled);
}

// Shutdown hook: disconnects the controllers and stops the Bluetooth stack, or, when the system restarts, keeps the
// stack up and schedules a reconnect.
static BOOL OnShutdown(BOOL final, u32 event) {
    WUDLibStatus status = WUDGetStatus();
    BOOL cleanup;

    if (final || status == WUD_LIB_STATUS_0) {
        return TRUE;
    }
    if (status == WUD_LIB_STATUS_4 || status == WUD_LIB_STATUS_1 || status == WUD_LIB_STATUS_2) {
        return FALSE;
    }
    if (WUDIsBusy()) {
        WUDCancelSyncDevice();
        return FALSE;
    }
    switch (event) {
    case 0:
        WPADRegisterAllocator(noAlloc, noFree);
        // FALLTHROUGH
    case 2:
    case 3:
        cleanup = TRUE;
        break;
    case 1:
    case 4:
    case 6:
        cleanup = FALSE;
        break;
    case 5:
        cleanup = __OSIsReturnToIdle ? TRUE : FALSE;
        break;
    }
    if (cleanup) {
        __WPADShutdown();
    } else {
        __WPADReconnect(TRUE);
    }
    return FALSE;
} // 0x8027D710

static OSShutdownFunctionInfo ShutdownFunctionInfo = {OnShutdown, 127};

// Sends one queued command to the controller as a HID output report and records what the reply has to fill in.
static void __SendData(s32 chan, WPADCommand command) {
    BOOL enabled;
    BOOL rumble;
    s8 devHandle;
    WPADControlBlock* cb;
    u8 reportId;
    BT_HDR* buf;
    u8* report;
    s32 status;
    u8* cmdData;
    u16 length;

    cb = _wpdcb[chan];
    reportId = command.reportId;
    cmdData = command.data;
    length = command.length;

    enabled = OSDisableInterrupts();
    devHandle = cb->devHandle;
    status = cb->status;

    if (devHandle < 0) {
        OSRestoreInterrupts(enabled);
        return;
    }

    cb->status = WPAD_ERR_BUSY;
    rumble = cb->motorRunning & _rumble;

    if (reportId == RPTID_SET_RUMBLE) {
        cb->status = status;
    } else if (reportId == RPTID_SEND_SPEAKER_DATA) {
        cb->status = status;
        cb->audioFrames--;
    } else {
        switch (reportId) {
        case RPTID_WRITE_DATA:
            break;
        case RPTID_READ_DATA:
            cb->wmReadErr = 0;
            cb->wmReadAddr = command.readAddress;
            cb->wmReadLength = command.readLength;
            cb->wmReadDataPtr = command.readBuf;
            break;
        case RPTID_REQUEST_STATUS:
            cb->status = status;
            cb->infoOut = command.info;
            cb->statusReqBusy = TRUE;
            break;
        default:
            cmdData[0] |= (1 << RPT_OUT_FLAG_REQUEST_ACK_RPT);
            break;
        }

        cb->cmdBlkCallback = command.callback;
        cb->lastReportId = reportId;
        cb->lastReportSendTime = __OSGetSystemTime() + OSSecondsToTicks(2);
        cb->cmdTimeoutAction = 0;
    }

    OSRestoreInterrupts(enabled);

    DEBUGPrint("handle = %d, repid = %02x\n", devHandle, reportId);

    buf = GKI_getbuf((u8)(length + 18));
    buf->len = (u8)(length + 1);
    buf->offset = 10;

    report = (u8*)buf + sizeof(BT_HDR) + buf->offset;
    report[0] = reportId;
    memcpy(&report[1], cmdData, length);

    if (rumble) {
        report[RPT_OUT_FLAGS] |= (1 << RPT_OUT_FLAG_RUMBLE);
    } else {
        report[RPT_OUT_FLAGS] &= ~(1 << RPT_OUT_FLAG_RUMBLE);
    }

    BTA_HhSendData(devHandle, buf);
} // 0x8027D8E0

// Folds the share of reports received in the last interval (packetCnt, per mille steps of 2) into the smoothed radio
// sensitivity and updates the link quality verdict with hysteresis.
static void WPADiRadioSensitivity(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    u32 two = 2;
    u16 sensitivity;

    if (_senseCnt != 10) {
        return;
    }

    sensitivity = cb->radioSensitivity * 9;
    sensitivity += (u16)(cb->packetCnt * 100 / two);
    sensitivity /= 10;
    sensitivity = MIN(100, sensitivity);

    cb->radioSensitivity = sensitivity;
    cb->packetCnt = 0;

    if (cb->radioQuality != WPAD_RADIO_QUALITY_GOOD) {
        if (sensitivity > 85) {
            cb->radioQuality = WPAD_RADIO_QUALITY_GOOD;
            cb->radioQualityOkMs = 0;
        } else if (sensitivity > 80) {
            cb->radioQualityOkMs++;
            if (cb->radioQualityOkMs >= 20) {
                cb->radioQuality = WPAD_RADIO_QUALITY_GOOD;
                cb->radioQualityOkMs = 0;
            }
        }
    } else if (sensitivity < 75) {
        cb->radioQuality = WPAD_RADIO_QUALITY_BAD;
        cb->radioQualityOkMs = 0;
    } else if (sensitivity < 80) {
        cb->radioQualityOkMs++;
        if (cb->radioQualityOkMs >= 1) {
            cb->radioQuality = WPAD_RADIO_QUALITY_BAD;
            cb->radioQualityOkMs = 0;
        }
    }
} // 0x8027DAC0

// Whether two samples of an input value differ.
static u8 IsButtonChanged(u16 lhs, u16 rhs) {
    return lhs != rhs ? TRUE : FALSE;
}

// Whether two analog readings differ by more than a threshold.
static u8 IsAnalogChanged(s32 lhs, s32 rhs, s32 threshold) {
    s32 diff = lhs - rhs < 0 ? rhs - lhs : lhs - rhs;
    return diff > threshold ? TRUE : FALSE;
}

// Debounces an analog change: only a run of differing samples counts as a change, and a run of equal samples makes
// the run counter decay.
static BOOL CalcAnalogNoise(WPADControlBlock* cb, WPADNZFilter filter, BOOL changed) {
    if (changed) {
        cb->filterDiff[filter]++;
        if (cb->filterDiff[filter] > _wpad_diff_count_threshold[filter]) {
            cb->filterDiff[filter] = 0;
            cb->filterSame[filter] = 0;
            return TRUE;
        }
    } else {
        cb->filterSame[filter] = (cb->filterSame[filter] + 1) % _wpad_hyst_count_threshold[filter];
        if (cb->filterSame[filter] == _wpad_hyst_count_threshold[filter] - 1 && cb->filterDiff[filter]) {
            cb->filterDiff[filter]--;
        }
    }
    return FALSE;
}

// Whether the newest sample of a controller differs from the previous one in any input (the auto-sleep timer
// restarts on a change).
static BOOL IsControllerDataChanged(WPADControlBlock* cb, void* pLhs, void* pRhs) {
    WPADStatus* lhs = pLhs;
    WPADStatus* rhs = pRhs;
    u8 changed = FALSE;

    if (cb->dataFormat != WPAD_FMT_UNK_14 && (lhs->err == WPAD_ERR_OK || lhs->err == WPAD_ERR_CORRUPTED) &&
        (rhs->err == WPAD_ERR_OK || rhs->err == WPAD_ERR_CORRUPTED)) {
        changed |= IsButtonChanged(lhs->button, rhs->button);
        changed |= CalcAnalogNoise(cb, WPAD_NZFILTER_ACC,
                                   IsAnalogChanged(lhs->accX, rhs->accX, 12) |
                                       IsAnalogChanged(lhs->accY, rhs->accY, 12) |
                                       IsAnalogChanged(lhs->accZ, rhs->accZ, 12));
        changed |= CalcAnalogNoise(cb, WPAD_NZFILTER_DPD,
                                   IsAnalogChanged(lhs->obj[0].x, rhs->obj[0].x, 2) |
                                       IsAnalogChanged(lhs->obj[0].y, rhs->obj[0].y, 2) |
                                       IsAnalogChanged(lhs->obj[1].x, rhs->obj[1].x, 2) |
                                       IsAnalogChanged(lhs->obj[1].y, rhs->obj[1].y, 2) |
                                       IsAnalogChanged(lhs->obj[2].x, rhs->obj[2].x, 2) |
                                       IsAnalogChanged(lhs->obj[2].y, rhs->obj[2].y, 2) |
                                       IsAnalogChanged(lhs->obj[3].x, rhs->obj[3].x, 2) |
                                       IsAnalogChanged(lhs->obj[3].y, rhs->obj[3].y, 2));
    }
    if (lhs->err == WPAD_ERR_OK && rhs->err == WPAD_ERR_OK) {
        switch (cb->dataFormat) {
        case WPAD_FMT_FS_BTN:
        case WPAD_FMT_FS_BTN_ACC:
        case WPAD_FMT_FS_BTN_ACC_DPD: {
            WPADFSStatus* lhsFS = pLhs;
            WPADFSStatus* rhsFS = pRhs;
            changed |= CalcAnalogNoise(cb, WPAD_NZFILTER_EXT,
                                       IsAnalogChanged(lhsFS->fsAccX, rhsFS->fsAccX, 12) |
                                           IsAnalogChanged(lhsFS->fsAccY, rhsFS->fsAccY, 12) |
                                           IsAnalogChanged(lhsFS->fsAccZ, rhsFS->fsAccZ, 12));
            changed |= IsAnalogChanged(lhsFS->fsStickX, rhsFS->fsStickX, 1);
            changed |= IsAnalogChanged(lhsFS->fsStickY, rhsFS->fsStickY, 1);
            break;
        }
        case WPAD_FMT_CLASSIC_BTN:
        case WPAD_FMT_CLASSIC_BTN_ACC:
        case WPAD_FMT_CLASSIC_BTN_ACC_DPD:
        case WPAD_FMT_GUITAR:
        case WPAD_FMT_DRUM: {
            WPADCLStatus* lhsCL = pLhs;
            WPADCLStatus* rhsCL = pRhs;
            s32 leftDiv, rightDiv, triggerDiv;

            // The stick and trigger resolution depends on the report mode of the extension.
            switch (cb->devMode) {
            case WPAD_DEV_MODE_CLASSIC_REDUCED:
                leftDiv = 16;
                rightDiv = 32;
                triggerDiv = 8;
                break;
            case WPAD_DEV_MODE_CLASSIC_STANDARD:
                leftDiv = 4;
                rightDiv = 4;
                triggerDiv = 1;
                break;
            default:
                leftDiv = 1;
                rightDiv = 1;
                triggerDiv = 1;
                break;
            }
            changed |= IsButtonChanged(lhsCL->clButton, rhsCL->clButton);
            changed |= IsAnalogChanged(lhsCL->clLStickX / leftDiv, rhsCL->clLStickX / leftDiv, 1);
            changed |= IsAnalogChanged(lhsCL->clLStickY / leftDiv, rhsCL->clLStickY / leftDiv, 1);
            changed |= IsAnalogChanged(lhsCL->clRStickX / rightDiv, rhsCL->clRStickX / rightDiv, 1);
            changed |= IsAnalogChanged(lhsCL->clRStickY / rightDiv, rhsCL->clRStickY / rightDiv, 1);
            changed |= IsAnalogChanged(lhsCL->clTriggerL / triggerDiv, rhsCL->clTriggerL / triggerDiv, 1);
            changed |= IsAnalogChanged(lhsCL->clTriggerR / triggerDiv, rhsCL->clTriggerR / triggerDiv, 1);
            break;
        }
        case WPAD_FMT_TRAIN: {
            WPADTRStatus* lhsTR = pLhs;
            WPADTRStatus* rhsTR = pRhs;
            changed |= IsButtonChanged(lhsTR->trButton, rhsTR->trButton);
            changed |= IsAnalogChanged(lhsTR->brake, rhsTR->brake, 1);
            changed |= IsAnalogChanged(lhsTR->mascon, rhsTR->mascon, 1);
            break;
        }
        case WPAD_FMT_BALANCE_CHECKER: {
            WPADBLStatus* lhsBL = pLhs;
            WPADBLStatus* rhsBL = pRhs;
            changed |= CalcAnalogNoise(cb, WPAD_NZFILTER_EXT,
                                       IsAnalogChanged(lhsBL->press[0], rhsBL->press[0], 50) |
                                           IsAnalogChanged(lhsBL->press[1], rhsBL->press[1], 50) |
                                           IsAnalogChanged(lhsBL->press[2], rhsBL->press[2], 50) |
                                           IsAnalogChanged(lhsBL->press[3], rhsBL->press[3], 50));
            break;
        }
        case WPAD_FMT_VSM:
        case WPAD_FMT_UNK_14: {
            WPADVSStatus* lhsVS = pLhs;
            WPADVSStatus* rhsVS = pRhs;
            int i;

            changed |= IsButtonChanged(lhsVS->unknown46, rhsVS->unknown46);
            changed |= IsAnalogChanged(lhsVS->unknown44, rhsVS->unknown44, 32);
            for (i = 0; i < 5; i++) {
                changed |= IsAnalogChanged(lhsVS->unknown2A[i], lhsVS->unknown36[i], 32);
            }
            break;
        }
        }
    }
    return changed;
} // 0x8027DBD0

// Leaves the combination counter running while A+B+Plus+Minus are held; holding them long enough makes the
// controller recalibrate.
static inline void WPADRecalibrate(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();

    cb->calibrated = FALSE;
    cb->recalibrateCount = 0;
    OSRestoreInterrupts(enabled);
}

// Counts the samples in which the recalibration button combination (A, B, + and -) is held and recalibrates the
// channel when it was held long enough.
static void CheckButtonCombination(s32 chan, void* status) {
    WPADControlBlock* cb = _wpdcb[chan];
    WPADCLStatus* sample = status;
    BOOL pressed = FALSE;

    if ((cb->dataFormat == WPAD_FMT_FS_BTN || cb->dataFormat == WPAD_FMT_FS_BTN_ACC ||
         cb->dataFormat == WPAD_FMT_FS_BTN_ACC_DPD) &&
        sample->base.button == (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_PLUS | WPAD_BUTTON_MINUS)) {
        pressed = TRUE;
    }
    if ((cb->dataFormat == WPAD_FMT_CLASSIC_BTN || cb->dataFormat == WPAD_FMT_CLASSIC_BTN_ACC ||
         cb->dataFormat == WPAD_FMT_CLASSIC_BTN_ACC_DPD) &&
        (sample->base.button == (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_PLUS | WPAD_BUTTON_MINUS) ||
         (sample->clButton == (WPAD_CL_BUTTON_A | WPAD_CL_BUTTON_B | WPAD_CL_BUTTON_PLUS | WPAD_CL_BUTTON_MINUS) &&
          sample->base.err == WPAD_ERR_OK))) {
        pressed = TRUE;
    }
    cb->recalibrateCount += pressed;
    if (cb->recalibrateCount > 600) {
        WPADRecalibrate(chan);
    }
} // 0x8027E4D0

// Connection status of a channel, read with interrupts off.
static inline s32 WPADiGetStatus(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    s32 status = cb->status;

    OSRestoreInterrupts(enabled);
    return status;
}

// Copies the Bluetooth address of the channel's controller (all zero without one).
static inline void WPADGetAddress(s32 chan, BD_ADDR_PTR addr) {
    WPADControlBlock* cb = _wpdcb[chan];
    BD_ADDR_PTR devAddr;
    BOOL enabled;
    s8 devHandle;

    enabled = OSDisableInterrupts();
    devHandle = cb->devHandle;
    OSRestoreInterrupts(enabled);

    devAddr = _WUDGetDevAddr(devHandle);
    if (devAddr != NULL) {
        memcpy(addr, devAddr, BD_ADDR_LEN);
    } else {
        memset(addr, 0, BD_ADDR_LEN);
    }
}

static void DisconnectCallback(s32 chan, s32 result);

// Drops the Bluetooth link of the channel's controller: at once, or (sleep) by switching its LEDs off first.
static inline void WPADiDisconnect(s32 chan, BOOL sleep) {
    WPADControlBlock* cb = _wpdcb[chan];
    s32 status;
    BOOL enabled;

    status = WPADiGetStatus(chan);
    if (status == WPAD_ERR_NO_CONTROLLER) {
        return;
    }

    if (sleep) {
        enabled = OSDisableInterrupts();
        if (cb->disconnect) {
            OSRestoreInterrupts(enabled);
            return;
        }
        cb->disconnect = TRUE;
        OSRestoreInterrupts(enabled);
        WPADControlLed(chan, 0, DisconnectCallback);
    } else {
        BD_ADDR addr;

        WPADGetAddress(chan, addr);
        btm_remove_acl(addr);
    }
}

// Runs every fifth manager tick: restarts the auto-sleep timer when the controller was used, disconnects an idle
// one when the timer runs out, and keeps the screen saver off while the controller is in use.
static void WPADiCheckContInputs(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL changed;
    BOOL screenSaver = FALSE;
    BOOL enabled;
    u8 rxBufIndex;
    WPADStatus* status;
    WPADStatus* previous;

    if (_checkCnt != 5) {
        return;
    }

    enabled = OSDisableInterrupts();
    rxBufIndex = cb->rxBufIndex != 0 ? 0 : 1;
    status = &cb->rxBufs[rxBufIndex].base;
    OSRestoreInterrupts(enabled);

    changed = IsControllerDataChanged(cb, status, cb->prBuf);
    CheckButtonCombination(chan, status);

    if (changed) {
        screenSaver = TRUE;
        cb->lastUpdateTime = __OSGetSystemTime();
        memcpy(cb->prBuf, status, WPAD_RX_DATASIZE);
    } else if (_sleepTime != 0) {
        s32 idle = OSTicksToSeconds(__OSGetSystemTime() - cb->lastUpdateTime);
        if (idle > _sleepTime * 60) {
            WPADiDisconnect(chan, FALSE);
        }
    }

    previous = &cb->prBuf[0].base;
    if (previous->err != WPAD_ERR_OK && previous->err != WPAD_ERR_CORRUPTED) {
        memcpy(cb->prBuf, status, WPAD_RX_DATASIZE);
    }

    if (screenSaver) {
        __VIResetRFIdle();
    }
} // 0x8027E590

// Number of commands waiting in a queue.
static s8 __GetCmdNumber(WPADCmdQueue* queue) {
    BOOL enabled;
    s8 num;

    enabled = OSDisableInterrupts();
    num = (s8)(queue->tail - queue->head);
    if (num < 0) {
        num += queue->capacity;
    }
    OSRestoreInterrupts(enabled);
    return num;
}

// Copies the oldest command of a queue without removing it.
static BOOL WPADiGetCommand(WPADCmdQueue* queue, WPADCommand* command) {
    BOOL enabled;

    if (__GetCmdNumber(queue) == 0) {
        return FALSE;
    }
    enabled = OSDisableInterrupts();
    memcpy(command, &queue->commands[queue->head], sizeof(WPADCommand));
    OSRestoreInterrupts(enabled);
    return TRUE;
}

// Removes the oldest command of a queue.
static BOOL WPADiPopCommand(WPADCmdQueue* queue) {
    BOOL enabled = OSDisableInterrupts();

    if (__GetCmdNumber(queue) == 0) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    memset(&queue->commands[queue->head], 0, sizeof(WPADCommand));
    queue->head = queue->head == queue->capacity - 1 ? 0 : queue->head + 1;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

// Sends a command if the previous request has finished; a controller that has not answered for more than a second
// is disconnected.
static s32 WPADiSendData(s32 chan, WPADCommand command) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    s32 status = cb->statusReqBusy ? WPAD_ERR_BUSY : WPADiGetStatus(chan);

    OSRestoreInterrupts(enabled);
    if (status == WPAD_ERR_OK) {
        __SendData(chan, command);
    }
    if (status == WPAD_ERR_BUSY) {
        if ((s32)OSTicksToSeconds(__OSGetSystemTime() - cb->lastReportSendTime) > 1 && _sleepTime != 0) {
            cb->lastReportSendTime = __OSGetSystemTime();
            WPADiDisconnect(chan, FALSE);
        }
    }
    return status;
}

// Sends the next command of the extension queue (one in five ticks, and only once an extension is attached,
// except for the data format change that detaches one).
static BOOL WPADiProcessExtCommand(s32 chan, BOOL prevSuccess) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL success = FALSE;
    WPADCommand command;

    if (prevSuccess) {
        return TRUE;
    }
    if (_extCnt[chan] != 5) {
        goto end;
    }
    if (!WPADiGetCommand(&cb->extCmdQueue, &command)) {
        goto end;
    }
    if (command.reportId != RPTID_SET_DATA_REPORT_MODE && !cb->info.attach) {
        goto end;
    }
    if (WPADiSendData(chan, command) != WPAD_ERR_OK) {
        goto end;
    }
    WPADiPopCommand(&cb->extCmdQueue);
    success = TRUE;
    _extCnt[chan] = 0;
end:
    _extCnt[chan] = _extCnt[chan] == 5 ? _extCnt[chan] : _extCnt[chan] + 1;
    return success;
}

// Sends the next command of the standard queue.
static BOOL WPADiProcessCommand(s32 chan, BOOL prevSuccess) {
    WPADControlBlock* cb = _wpdcb[chan];
    WPADCommand command;

    if (prevSuccess) {
        return TRUE;
    }
    if (WPADiGetCommand(&cb->stdCmdQueue, &command) && WPADiSendData(chan, command) == WPAD_ERR_OK) {
        WPADiPopCommand(&cb->stdCmdQueue);
        return TRUE;
    }
    return FALSE;
}

// Pointer sensor sensitivity of the system configuration, clamped to the supported range.
static u8 __GetDpdSensitivity(void) {
    u8 level = SCGetBtDpdSensibility();

    if (level < WPAD_MIN_DPD_SENS) {
        level = WPAD_MIN_DPD_SENS;
    }
    if (level > WPAD_MAX_DPD_SENS) {
        level = WPAD_MAX_DPD_SENS;
    }
    return level;
}

// Whether the system configuration puts the sensor bar above the screen.
static u8 __GetSensorBarPosition(void) {
    return !!(SCGetWpadSensorBarPosition() == SC_SENSOR_BAR_TOP);
}

// Whether the system configuration has the rumble motor enabled.
static u32 __GetMotorMode(void) {
    return !!(SCGetWpadMotorMode() == SC_MOTOR_ON);
}

// Limits a speaker volume to the range the controller accepts.
static u8 __ClampSpeakerVolume(u8 volume) {
    u8 result = volume;

    if (volume <= 0) {
        result = 0;
    }
    if (volume >= WPAD_MAX_SPEAKER_VOLUME) {
        result = WPAD_MAX_SPEAKER_VOLUME;
    }
    return result;
}

// Speaker volume of the system configuration.
static u8 __GetSpeakerVolume(void) {
    u8 volume = SCGetWpadSpeakerVolume();
    return __ClampSpeakerVolume(volume);
}

// Picks up the sensor bar, rumble and speaker settings from the system configuration once it is readable.
static void WPADiGetScSettings(void) {
    if (SCCheckStatus() != SC_STATUS_OK) {
        return;
    }
    WUDUpdateSCSetting();
    if (_scSetting) {
        _dpdSensitivity = __GetDpdSensitivity();
        _sensorBarPos = __GetSensorBarPosition();
        _rumble = __GetMotorMode();
        _speakerVolume = __GetSpeakerVolume();
        _scSetting = FALSE;
    }
}

// Re-reads the channel the WiFi module uses (once per minute) and keeps the Bluetooth hopping away from it.
static void WPADiAfh(void) {
    BOOL enabled;
    u8* wifiChannel;

    if (_afhCnt != 60000) {
        return;
    }

    wifiChannel = OSPhysicalToCached(OS_PHYS_WIFI_AFH_CHANNEL);
    DCInvalidateRange(wifiChannel, sizeof *wifiChannel);

    DEBUGPrint("Check the update of WiFi using channel\n");
    DEBUGPrint("WiFi uses channel = %d\n", *wifiChannel);

    if (_afhChannel != *wifiChannel) {
        enabled = OSDisableInterrupts();
        _afhChannel = *wifiChannel;
        OSRestoreInterrupts(enabled);
        WUDSetDisableChannel(_afhChannel);
    }
}

// Stops the motor with a rumble-off report unless commands are queued (they carry the motor bit themselves).
static void WPADiRumbleMotor(s32 chan, BOOL prevSuccess) {
    WPADCommand command;

    if (prevSuccess == TRUE || __GetCmdNumber(&_wpdcb[chan]->stdCmdQueue) > 0) {
        _wpdcb[chan]->motorBusy = FALSE;
    } else if (_rumbleCnt[chan] == 5) {
        _wpdcb[chan]->motorBusy = FALSE;

        command.reportId = RPTID_SET_RUMBLE;
        command.length = 1;
        command.data[0] = FALSE;
        command.callback = NULL;
        __SendData(chan, command);
    }

    _rumbleCnt[chan] = _wpdcb[chan]->motorBusy ? _rumbleCnt[chan] + 1 : 0;
}

// Resets the Bluetooth module and stops the stack so that it can be started again.
static void WPADiResetModule(BOOL saveSimpleDevs) {
    DEBUGPrint("Reconnect Start!!\n");
    BTA_DmSendHciReset();
    OSCancelAlarm(&_managerAlarm);
    WUDSetHidRecvCallback(NULL);
    WUDShutdown(saveSimpleDevs);
}

// Counts down to the restart requested by __WPADReconnect.
static BOOL WPADiCheckReconnect(void) {
    if (_recFlag >= 0) {
        if (--_recCnt <= 0) {
            WPADiResetModule(_recFlag);
        }
        return TRUE;
    }
    return FALSE;
}

// The manager: runs from the alarm every millisecond on its own stack and drives all four channels.
static void WPADiManageHandler(OSAlarm* alarm, OSContext* context) {
    WUDLibStatus status = WUDGetStatus();
    s32 chan;
    BOOL success;

    if (status != WUD_LIB_STATUS_3) {
        if (status == WUD_LIB_STATUS_2 && !_initialized) {
            _initialized = TRUE;
            _recCnt = 50;
            WUDSetHidConnCallback(WPADiConnCallback);
            WUDSetHidRecvCallback(WPADiRecvCallback);
        }
        return;
    }

    if (WPADiCheckReconnect()) {
        return;
    }
    WPADiAfh();
    WPADiGetScSettings();

    for (chan = WPAD_CHAN0; chan < WPAD_MAX_CONTROLLERS; chan++) {
        success = FALSE;
        if (_wpdcb[chan]->used) {
            success |= WPADiProcessExtCommand(chan, success);
            success |= WPADiProcessCommand(chan, success);
            WPADiRumbleMotor(chan, success);
            WPADiCheckContInputs(chan);
            WPADiRadioSensitivity(chan);
        }
        if (_wpdcb[chan]->extWasDisconnected && _wpdcb[chan]->reconnectExtMs-- < 0) {
            _wpdcb[chan]->extWasDisconnected = FALSE;
        }
    }

    _senseCnt = _senseCnt == 10 ? 0 : _senseCnt + 1;
    _checkCnt = _checkCnt == 5 ? 0 : _checkCnt + 1;
    _afhCnt = _afhCnt == 60000 ? 0 : _afhCnt + 1;
    BTA_HhGetAclQueueInfo();
} // 0x8027E730

// Alarm handler: switches to the manager's own stack.
static void WPADiManageHandler0(OSAlarm* alarm, OSContext* context) {
    OSSwitchFiberEx((u32)alarm, (u32)context, 0, 0, (u32)WPADiManageHandler,
                    (u32)(__WPADiManageHandlerStack + sizeof(__WPADiManageHandlerStack)));
} // 0x8027F110

// Puts the channel back into the state of an unused control block.
static void __ClearControlBlock(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    WPADStatus* status;
    int filter;

    cb->infoOut = NULL;

    cb->motorRunning = FALSE;

    cb->cmdBlkCallback = NULL;
    cb->extensionCallback = NULL;
    cb->samplingCallback = NULL;

    cb->samplingBuf = NULL;
    cb->samplingBufIndex = 0;
    cb->samplingBufLength = 0;

    cb->dataFormat = WPAD_FMT_CORE_BTN;
    cb->status = WPAD_ERR_NO_CONTROLLER;
    cb->devType = WPAD_DEV_NONE;
    cb->devMode = WPAD_DEV_MODE_NORMAL;

    cb->calibrated = FALSE;
    cb->recalibrateCount = 0;

    cb->statusReqBusy = FALSE;

    cb->defaultDpdSize = 12;
    cb->currentDpdCommand = WPAD_DPD_DISABLE;
    cb->pendingDpdCommand = 0;

    for (filter = 0; filter < WPAD_MAX_NZFILTERS; filter++) {
        cb->filterDiff[filter] = 0;
        cb->filterSame[filter] = 0;
    }

    cb->lastUpdateTime = __OSGetSystemTime();
    cb->lastReportSendTime = __OSGetSystemTime();
    cb->cmdTimeoutAction = 0;
    cb->field_0x8C9 = 0;
    cb->field_0x8C4 = 0;
    cb->wmReadDataPtr = NULL;
    cb->wmReadAddr = 0;
    cb->wmReadLength = 0;
    cb->wmReadErr = 0;
    cb->devHandle = WUD_DEV_HANDLE_INVALID;
    cb->used = FALSE;
    cb->handshakeFinished = FALSE;
    cb->oldFw = 0;
    cb->radioQuality = WPAD_RADIO_QUALITY_BAD;
    cb->radioQualityOkMs = 0;
    cb->audioFrames = 0;
    cb->keyIdx = 0;
    cb->radioSensitivity = 0;
    cb->packetCnt = 0;
    cb->disconnect = TRUE;
    cb->getInfoBusy = FALSE;
    cb->getInfoCB = NULL;
    cb->savePower = FALSE;
    cb->blcBattery = WPAD_BATTERY_LEVEL_MAX;
    cb->savedDevType = WPAD_DEV_NONE;
    cb->extWasDisconnected = FALSE;
    cb->reconnectExtMs = 0;

    memset(&cb->info, 0, sizeof(cb->info));
    memset(&cb->wmReadDataBuf, 0, sizeof(cb->wmReadDataBuf));

    memset(&cb->devConf, 0, sizeof(WPADDevConfig));
    memset(&cb->extConf, 0, sizeof(WPADExtConfig));
    memset(&cb->key, 0, sizeof(cb->key));
    memset(&cb->ft, 0, sizeof(cb->ft));
    memset(&cb->sb, 0, sizeof(cb->sb));
    memset(&cb->gameInfo, 0, sizeof(WPADMEMGameInfo));

    cb->rxBufIndex = 0;
    memset(cb->rxBufs, 0, sizeof(cb->rxBufs));
    status = &cb->rxBufs[0].base;
    status->err = WPAD_ERR_NO_CONTROLLER;
    status = &cb->rxBufs[1].base;
    status->err = WPAD_ERR_NO_CONTROLLER;
    memcpy(cb->prBuf, &cb->rxBufs[0], sizeof(cb->prBuf));

    cb->gameInfoErr[0] = -1;
    cb->gameInfoErr[1] = -1;

    cb->stdCmdQueue.commands = cb->stdCmdQueueList;
    cb->stdCmdQueue.capacity = WPAD_COMMAND_CMD_MAX_LEN;
    cb->extCmdQueue.commands = cb->extCmdQueueList;
    cb->extCmdQueue.capacity = WPAD_COMMAND_EXT_MAX_LEN;

    WPADiClearQueue(&cb->stdCmdQueue);
    WPADiClearQueue(&cb->extCmdQueue);

    WPADiClearMemBlock(chan);
    _extCnt[chan] = 0;
    _rumbleCnt[chan] = 0;
} // 0x8027F130

// Hollywood GPIO output register and the bit that powers the sensor bar.
#define ACR_GPIO1BOUT 0xC0
#define GPIO_SENSORBAR (1 << 8)

// Switches the sensor bar on or off.
static inline void WPADSetSensorBarPower(BOOL enable) {
    u32 reg;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    reg = __ACRRestrictedRegs[ACR_GPIO1BOUT / 4];

    if (enable) {
        reg |= GPIO_SENSORBAR;
    } else {
        reg &= ~GPIO_SENSORBAR;
    }

    __ACRRestrictedRegs[ACR_GPIO1BOUT / 4] = reg;
    OSRestoreInterrupts(enabled);
}

// Brings the driver up: the sensor bar, the control blocks, the settings, the extension libraries' hooks and the
// manager alarm.
static void WPADiInitSub(void) {
    int i;
    s32 chan;

    WPADSetSensorBarPower(TRUE);

    for (i = 0; i < WUD_MAX_DEV_ENTRY; i++) {
        _dev_handle_index[i] = WUD_DEV_HANDLE_INVALID;
    }

    DEBUGPrint("WPADInit()\n");

    for (chan = 0; chan < WPAD_MAX_CONTROLLERS; chan++) {
        _wpdcb[chan] = &_wpd[chan];
        _chan_active_state[chan] = FALSE;

        _wpdcb[chan]->connectCallback = NULL;
        __ClearControlBlock(chan);
        OSInitThreadQueue(&_wpd[chan].threadQueue);

        _extCnt[chan] = 0;
        _rumbleCnt[chan] = 0;
    }

    _sleepTime = 5;
    _gamecode = OSGetAppGamename();
    _gametype = OSGetAppType();
    _dpdSensitivity = __GetDpdSensitivity();
    _sensorBarPos = __GetSensorBarPosition();
    _rumble = __GetMotorMode();
    _speakerVolume = __GetSpeakerVolume();
    _senseCnt = 0;
    _checkCnt = 0;
    _afhCnt = 0;
    _shutdown = FALSE;
    _scSetting = TRUE;
    _afhChannel = -1;
    _callbackByKPAD = FALSE;
    OSRegisterVersion(__WPADVersion);
    if (_wpadVSMInit != NULL) {
        _wpadVSMInit();
    }
    if (_wpadTRNInit != NULL) {
        _wpadTRNInit();
    }
    if (_wpadGTRInit != NULL) {
        _wpadGTRInit();
    }
    if (_wpadDRMInit != NULL) {
        _wpadDRMInit();
    }

    OSCreateAlarm(&_managerAlarm);
    OSSetPeriodicAlarm(&_managerAlarm, OSGetTime(), OSMillisecondsToTicks(1), WPADiManageHandler0);
} // 0x8027F3B0

// Starts the library: registers the shutdown hook, initialises the Bluetooth stack and the channels.
void WPADInit(void) {
    _startup = TRUE;
    if (!_regShutdown) {
        OSRegisterShutdownFunction(&ShutdownFunctionInfo);
        _regShutdown = TRUE;
    }
    if (WUDInit()) {
        _initialized = FALSE;
        _recFlag = -1;
        _recCnt = 50;
        WPADiInitSub();
    }
} // 0x8027F610

BOOL WPADStartSimpleSync(void) {
    return WUDStartSyncSimple();
} // no copy in retail (dead-stripped)

BOOL WPADStartFastSimpleSync(void) {
    return WUDStartFastSyncSimple();
} // 0x8027F680

BOOL WPADStopSimpleSync(void) {
    return WUDStopSyncSimple();
} // 0x8027F690

WPADSyncDeviceCallback WPADSetSimpleSyncCallback(WPADSyncDeviceCallback callback) {
    return WUDSetSyncSimpleCallback(callback);
} // 0x8027F6A0

// Installs the memory allocator of the Bluetooth stack.
void WPADRegisterAllocator(WPADAlloc alloc, WPADFree free) {
    WUDRegisterAllocator(alloc, free);
} // 0x8027F6B0

s32 WPADGetStatus(void) {
    return WUDGetStatus();
} // 0x8027F6C0

// Radio quality of the channel, as a percentage.
u8 WPADGetRadioSensitivity(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    u8 sensitivity = cb->radioSensitivity;
    OSRestoreInterrupts(enabled);
    return sensitivity;
} // 0x8027F6D0

// Sensor bar position of the system configuration (WPAD_SENSOR_BAR_BOTTOM or _TOP).
u8 WPADGetSensorBarPosition(void) {
    BOOL enabled;
    u8 pos;

    enabled = OSDisableInterrupts();
    pos = _sensorBarPos;
    OSRestoreInterrupts(enabled);
    return pos;
} // 0x8027F720

// Completion of the setup commands queued by firmwareCheckCallback: once the controller answered them, the
// connection counts as established. A failure drops the link.
static void setupCallback(s32 chan, s32 result) {
    WPADControlBlock* cb = _wpdcb[chan];

    if (result == WPAD_CESUCCESS) {
        cb->handshakeFinished = TRUE;
        if (cb->connectCallback != NULL) {
            cb->connectCallback(chan, result);
        }
    } else {
        WPADiDisconnect(chan, FALSE);
    }
} // 0x8027F760

// Completion of the other setup commands: a failure empties the command queue and drops the link.
static void abortConnCallback(s32 chan, s32 result) {
    WPADControlBlock* cb = _wpdcb[chan];

    if (result != WPAD_CESUCCESS) {
        WPADiClearQueue(&cb->stdCmdQueue);
        WPADiDisconnect(chan, FALSE);
    }
} // 0x8027F830

// Pushes a command onto a queue; fails when the queue is full.
static BOOL WPADiPushCommand(WPADCmdQueue* queue, WPADCommand command) {
    BOOL enabled = OSDisableInterrupts();

    if (queue->capacity - 1 == __GetCmdNumber(queue)) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    memset(&queue->commands[queue->tail], 0, sizeof(WPADCommand));
    memcpy(&queue->commands[queue->tail], &command, sizeof(WPADCommand));
    queue->tail = queue->tail == queue->capacity - 1 ? 0 : queue->tail + 1;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

// Queues the "set LEDs" output report.
static BOOL WPADiSendSetPort(WPADCmdQueue* queue, u8 port, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_SET_PORT;
    command.length = RPT11_SIZE;
    command.data[0] = port << 4;
    command.callback = callback;
    return WPADiPushCommand(queue, command);
}

// Queues the output report that switches the pointer sensor clock (DPD CSB) on or off.
static BOOL WPADiSendDPDCSB(WPADCmdQueue* queue, BOOL enable, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_SEND_DPD_CSB;
    command.length = RPT1A_SIZE;
    command.data[0] = enable ? 4 : 0;
    command.callback = callback;
    return WPADiPushCommand(queue, command);
}

// Queues a status request; the answer is stored in `info` when it is not NULL.
static BOOL WPADiSendGetContStat(WPADCmdQueue* queue, WPADInfo* info, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_REQUEST_STATUS;
    command.length = RPT15_SIZE;
    command.data[0] = 0;
    command.callback = callback;
    command.info = info;
    return WPADiPushCommand(queue, command);
}

// Completion of the first read of a new connection: a controller that cannot read the 0x1770 register runs the
// NDEV 2.0 firmware, which keeps its configuration elsewhere. Queues the rest of the connection setup.
static void firmwareCheckCallback(s32 chan, s32 result) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    u32 address;
    u16 size;
    u8 port;

    if (result == WPAD_ERR_NO_CONTROLLER) {
        return;
    }

    enabled = OSDisableInterrupts();
    cb->oldFw = result == WPAD_CESUCCESS ? 1 : 0;
    cb->status = WPAD_ERR_OK;
    OSRestoreInterrupts(enabled);

    size = result == WPAD_CESUCCESS ? 20 : 42;
    address = result == WPAD_CESUCCESS ? WM_ADDR_MEM_176C : WM_ADDR_MEM_DEV_CONFIG_0;

    DEBUGPrint(" ==>this error means that the firmware is for NDEV %s\n", cb->oldFw != 0 ? "2.0" : "2.1 or later");

    port = cb->devType == WPAD_DEV_BALANCE_CHECKER ? 1 : 1 << chan;

    WPADiSendSetReportType(&cb->stdCmdQueue, WPAD_FMT_CORE_BTN, cb->savePower, abortConnCallback);
    WPADiSendDPDCSB(&cb->stdCmdQueue, FALSE, abortConnCallback);
    WPADiSendSetPort(&cb->stdCmdQueue, port, abortConnCallback);
    WPADiSendReadData(&cb->stdCmdQueue, cb->wmReadDataBuf, sizeof(WPADMEMGameInfo), WM_ADDR_MEM_GAME_INFO_0,
                      abortConnCallback);
    WPADiSendReadData(&cb->stdCmdQueue, cb->wmReadDataBuf, sizeof(WPADMEMGameInfo), WM_ADDR_MEM_GAME_INFO_1,
                      abortConnCallback);
    WPADiSendReadData(&cb->stdCmdQueue, cb->wmReadDataBuf, size, address, setupCallback);
    WPADiSendGetContStat(&cb->stdCmdQueue, NULL, NULL);
} // 0x8027F920

// Queues the report that switches the pointer sensor (camera) on or off.
static BOOL WPADiSendEnableDPD(WPADCmdQueue* queue, BOOL enable, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_ENABLE_DPD;
    command.length = RPT13_SIZE;
    command.data[0] = enable ? 4 : 0;
    command.callback = callback;
    return WPADiPushCommand(queue, command);
}

// Queues one block of speaker sample data (the length goes into the upper five bits of the first byte).
static BOOL WPADiSendStreamData(WPADCmdQueue* queue, void* data, u16 len) {
    WPADCommand command;
    u8 packedLen = len << 3;

    command.reportId = RPTID_SEND_SPEAKER_DATA;
    command.length = RPT18_SIZE;
    command.data[RPT18_DATA_LENGTH] = packedLen;
    command.callback = NULL;
    memcpy(&command.data[RPT18_DATA], data, len);
    return WPADiPushCommand(queue, command);
}

// Queues the speaker mute report.
static BOOL WPADiSendMuteSpeaker(WPADCmdQueue* queue, BOOL mute, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_MUTE_SPEAKER;
    command.length = RPT19_SIZE;
    command.data[0] = mute ? 4 : 0;
    command.callback = callback;
    return WPADiPushCommand(queue, command);
}

// Queues the report that powers the speaker on or off.
static BOOL WPADiSendEnableSpeaker(WPADCmdQueue* queue, BOOL enable, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_ENABLE_SPEAKER;
    command.length = RPT14_SIZE;
    command.data[0] = enable ? 4 : 0;
    command.callback = callback;
    return WPADiPushCommand(queue, command);
}

// Whether a queue has room for `num` more commands.
static BOOL __CanPushCmdQueue(WPADCmdQueue* queue, s8 num) {
    s8 queueSize = __GetCmdNumber(queue);

    if ((u32)(queueSize + num) <= queue->capacity - 1) {
        return TRUE;
    } else {
        return FALSE;
    }
}

// Picks the channel for a newly connected controller: the Balance Board always takes channel 3, a controller that
// was the last one used on a free channel gets that channel back, otherwise the first free one is used.
static s32 WPADiRetrieveChannel(WUDDevInfo* devInfo) {
    BD_ADDR_PTR addr;
    s32 i;
    s32 chan = WPAD_CHAN_INVALID;
    u8 devHandle = devInfo->devHandle;

    addr = _WUDGetDevAddr(devHandle);
    if (memcmp(devInfo->conf.devName, "Nintendo RVL-WBC", 16) == 0) {
        chan = WPAD_CHAN3;
        if (_wpdcb[chan]->used) {
            btm_remove_acl(addr);
            return WPAD_CHAN_INVALID;
        }
    } else {
        for (i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            if (WUDIsLatestDevice(i, addr) && !_chan_active_state[i]) {
                chan = i;
                break;
            }
            if (!_chan_active_state[i] && chan < WPAD_CHAN0) {
                chan = i;
            }
        }
    }
    if (!WUDIsLatestDevice(chan, addr)) {
        WUDSetDeviceHistory(chan, addr);
    }
    _chan_active_state[chan] = TRUE;
    return chan;
} // 0x80280220

// Connection callback of the Bluetooth layer. A new connection gets a channel and its control block set up, and
// starts the firmware check; a closed one completes all pending commands with "no controller" and frees the channel.
static void WPADiConnCallback(WUDDevInfo* devInfo, u8 open) {
    u8 devHandle = devInfo->devHandle;
    s32 chan;
    WPADControlBlock* cb;
    WPADCommand command;

    DEBUGPrint("connection is %s\n", open ? "opened" : "closed");
    if (open) {
        chan = WPADiRetrieveChannel(devInfo);
        if (chan < 0) {
            return;
        }
        _dev_handle_index[devHandle] = chan;
        __ClearControlBlock(chan);
        cb = _wpdcb[chan];
        if (memcmp(devInfo->conf.devName, "Nintendo RVL-CNT", 16) == 0) {
            cb->devType = WPAD_DEV_CORE;
            cb->dataFormat = WPAD_FMT_CORE_BTN;
        } else if (memcmp(devInfo->conf.devName, "Nintendo RVL-WBC", 16) == 0 && WUDIsLinkedWBC()) {
            cb->devType = WPAD_DEV_BALANCE_CHECKER;
            cb->dataFormat = WPAD_FMT_BALANCE_CHECKER;
        } else {
            cb->devType = WPAD_DEV_FUTURE;
            cb->dataFormat = WPAD_FMT_CORE_BTN;
        }
        cb->devHandle = devHandle;
        cb->used = TRUE;
        cb->status = WPAD_ERR_OK;
        cb->radioSensitivity = 100;
        cb->disconnect = FALSE;
        cb->extState = 0;
        cb->savedDevType = cb->devType;
        WPADiSendReadData(&cb->stdCmdQueue, cb->wmReadDataBuf, 1, WM_ADDR_MEM_1770, firmwareCheckCallback);
        __VIResetRFIdle();
    } else {
        chan = _dev_handle_index[devHandle];
        _dev_handle_index[devHandle] = WUD_DEV_HANDLE_INVALID;
        if (chan != WUD_DEV_HANDLE_INVALID) {
            cb = _wpdcb[chan];
            cb->status = WPAD_ERR_NO_CONTROLLER;
            if (cb->cmdBlkCallback != NULL) {
                cb->cmdBlkCallback(chan, WPAD_ERR_NO_CONTROLLER);
            } else if (cb->memBlock.callback != NULL) {
                cb->memBlock.callback(chan, WPAD_ERR_NO_CONTROLLER);
            }
            while (WPADiGetCommand(&cb->stdCmdQueue, &command)) {
                if (command.callback != NULL) {
                    command.callback(chan, WPAD_ERR_NO_CONTROLLER);
                }
                WPADiPopCommand(&cb->stdCmdQueue);
            }
            DEBUGPrint("clean up command queue\n");
            if (cb->samplingBuf != NULL) {
                WPADSetAutoSamplingBuf(chan, cb->samplingBuf, cb->samplingBufLength);
            }
            __ClearControlBlock(chan);
            _chan_active_state[chan] = FALSE;
            if (cb->connectCallback != NULL) {
                cb->connectCallback(chan, WPAD_ERR_NO_CONTROLLER);
            }
        } else {
            DEBUGPrint("WARNING: disconnection for device handle not assigned to channel.\n");
        }
    }
} // 0x80280340

// Receives an input report from the Bluetooth stack and hands it to the HID parser of the channel it belongs to.
void WPADiRecvCallback(u8 devHandle, u8* report, u16 length) {
    u8 chan;
    s32 err;

    chan = _dev_handle_index[devHandle];
    if (chan < WPAD_MAX_CONTROLLERS) {
        err = WPADiHIDParser(chan, report);
        if (err) {
            DEBUGPrint("HID Parser reports: %d\n", err);
        }
    }
} // 0x80280800

// Reads one gravity unit of the accelerometer (the reading at 1 g minus the reading at 0 g) from the calibration.
void WPADGetAccGravityUnit(s32 chan, u32 type, WPADAccGravityUnit* acc) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;

    enabled = OSDisableInterrupts();
    if (acc) {
        switch (type) {
        case WPAD_ACC_GRAVITY_UNIT_CORE:
            acc->x = (s16)(cb->devConf.acc1g.x - cb->devConf.acc0g.x);
            acc->y = (s16)(cb->devConf.acc1g.y - cb->devConf.acc0g.y);
            acc->z = (s16)(cb->devConf.acc1g.z - cb->devConf.acc0g.z);
            break;
        case WPAD_ACC_GRAVITY_UNIT_FS:
            acc->x = (s16)(cb->extConf.fs.acc1g.x - cb->extConf.fs.acc0g.x);
            acc->y = (s16)(cb->extConf.fs.acc1g.y - cb->extConf.fs.acc0g.y);
            acc->z = (s16)(cb->extConf.fs.acc1g.z - cb->extConf.fs.acc0g.z);
            break;
        }
    }
    OSRestoreInterrupts(enabled);
} // 0x80280850

// Completion of the "controller off" LED command that WPADiDisconnect sends to put the controller to sleep: the link
// is closed once the controller has acknowledged it.
static void DisconnectCallback(s32 chan, s32 result) {
    WPADControlBlock* cb = _wpdcb[chan];

    if (result != WPAD_ERR_NO_CONTROLLER) {
        BTA_HhClose(cb->devHandle);
    }
} // 0x80280930

// Disconnects a controller: it is forgotten as the last device of the channel and told to switch off.
void WPADDisconnect(s32 chan) {
    s32 status = WPADiGetStatus(chan);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        return;
    }
    WUDSetDeviceHistory(chan, NULL);
    WPADiDisconnect(chan, TRUE);
} // 0x80280960

// Sets the idle time in minutes after which controllers switch themselves off (0 keeps them on).
void WPADSetAutoSleepTime(u8 minute) {
    BOOL enabled = OSDisableInterrupts();
    _sleepTime = minute;
    OSRestoreInterrupts(enabled);
} // 0x80280A20

// Name unknown (called from game code): stamps the channel's last-input time, which feeds the auto-sleep timer.
void WPADResetAutoSleepTimeCount(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();

    cb->lastUpdateTime = __OSGetSystemTime();
    OSRestoreInterrupts(enabled);
} // 0x80280A60

// Connection status of a channel and, optionally, the type of the controller on it.
s32 WPADProbe(s32 chan, u32* devType) {
    WPADControlBlock* cb = _wpdcb[chan];
    s32 status;
    BOOL enabled = OSDisableInterrupts();

    if (devType != NULL) {
        *devType = cb->devType;
    }
    status = cb->status;
    if (status != WPAD_ERR_NO_CONTROLLER) {
        if (cb->devType == WPAD_DEV_NONE) {
            status = WPAD_ERR_NO_CONTROLLER;
        } else if (!cb->handshakeFinished) {
            status = WPAD_ERR_BUSY;
        }
    }
    OSRestoreInterrupts(enabled);
    return status;
} // 0x80280AC0

// Installs the callback that runs after every received sample; returns the previous one.
WPADSamplingCallback WPADSetSamplingCallback(s32 chan, WPADSamplingCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    WPADSamplingCallback prev;
    BOOL enabled;

    if (_callbackByKPAD) {
        OSReport("WARNING: Overwritten the callback needed by KPAD.\n");
        OSReport("         Please call KPADSetSamplingCallback instead of WPADSetSamplingCallback.\n");
    }

    enabled = OSDisableInterrupts();
    prev = cb->samplingCallback;
    cb->samplingCallback = callback;
    OSRestoreInterrupts(enabled);
    return prev;
} // 0x80280B50

// Installs the callback that reports connections and disconnections; returns the previous one.
WPADConnectCallback WPADSetConnectCallback(s32 chan, WPADConnectCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    WPADConnectCallback prev;
    BOOL enabled;

    if (_callbackByKPAD) {
        OSReport("WARNING: Overwritten the callback needed by KPAD.\n");
        OSReport("         Please call KPADSetConnectCallback instead of WPADSetConnectCallback.\n");
    }

    enabled = OSDisableInterrupts();
    prev = cb->connectCallback;
    cb->connectCallback = callback;
    OSRestoreInterrupts(enabled);
    return prev;
} // 0x80280BE0

// Installs the callback that reports a change of extension; returns the previous one.
WPADExtensionCallback WPADSetExtensionCallback(s32 chan, WPADExtensionCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    WPADExtensionCallback prev;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    prev = cb->extensionCallback;
    cb->extensionCallback = callback;
    OSRestoreInterrupts(enabled);
    return prev;
} // 0x80280C70

// Data format a channel reports in.
u32 WPADGetDataFormat(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    u32 format = cb->dataFormat;
    OSRestoreInterrupts(enabled);
    return format;
} // 0x80280CD0

// Changes the data format of a channel; the controller is told with a set-report-type command.
s32 WPADSetDataFormat(s32 chan, u32 format) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    s32 status;
    BOOL setup;
    u32 prevFormat;

    enabled = OSDisableInterrupts();
    setup = cb->handshakeFinished;
    status = cb->status;
    prevFormat = cb->dataFormat;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        return WPAD_ERR_NO_CONTROLLER;
    }
    if (!setup) {
        return WPAD_ERR_BUSY;
    }
    if (prevFormat == format) {
        return WPAD_ERR_OK;
    }
    if (WPADiSendSetReportType(&cb->stdCmdQueue, format, cb->savePower, NULL)) {
        enabled = OSDisableInterrupts();
        cb->dataFormat = format;
        OSRestoreInterrupts(enabled);
        return WPAD_ERR_OK;
    }
    return WPAD_ERR_BUSY;
} // 0x80280D20

// Completion of the WPADGetInfoAsync request: forwards the result to the caller's callback and frees the slot.
static void __infoCallback(s32 chan, s32 result) {
    WPADControlBlock* cb = _wpdcb[chan];

    if (cb->getInfoCB != NULL) {
        cb->getInfoCB(chan, result);
    }

    cb->getInfoCB = NULL;
    cb->getInfoBusy = FALSE;
} // 0x80280DE0

// Requests the status of a controller (battery, extension, LEDs); `info` is filled in before the callback runs.
s32 WPADGetInfoAsync(s32 chan, WPADInfo* info, WPADCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL handshake;
    s32 status;
    BOOL enabled;
    u8 getInfoBusy;

    enabled = OSDisableInterrupts();
    handshake = cb->handshakeFinished;
    status = cb->status;
    getInfoBusy = cb->getInfoBusy;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        goto end;
    }

    if (!handshake || getInfoBusy) {
        status = WPAD_ERR_BUSY;
        goto end;
    }

    enabled = OSDisableInterrupts();
    cb->getInfoBusy = TRUE;
    cb->getInfoCB = callback;
    OSRestoreInterrupts(enabled);

    if (WPADiSendGetContStat(&cb->stdCmdQueue, info, __infoCallback)) {
        status = WPAD_ERR_OK;
        goto end;
    }

    status = WPAD_ERR_BUSY;

    enabled = OSDisableInterrupts();
    cb->getInfoBusy = FALSE;
    cb->getInfoCB = NULL;
    OSRestoreInterrupts(enabled);

end:
    if (status != WPAD_ERR_OK && callback != NULL) {
        callback(chan, status);
    }

    return status;
} // 0x80280E40

// Starts or stops the rumble motor of a channel; the change is sent by the manager handler.
void WPADControlMotor(s32 chan, u32 command) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    s32 status;

    enabled = OSDisableInterrupts();
    status = cb->status;

    if (status == WPAD_ERR_NO_CONTROLLER) {
        OSRestoreInterrupts(enabled);
        return;
    }

    if (_rumble == FALSE) {
        if (command != WPAD_MOTOR_STOP || cb->motorRunning != TRUE) {
            OSRestoreInterrupts(enabled);
            return;
        }
    }

    if ((command == WPAD_MOTOR_STOP && cb->motorRunning == FALSE) ||
        (command == WPAD_MOTOR_RUMBLE && cb->motorRunning == TRUE)) {
        OSRestoreInterrupts(enabled);
        return;
    }

    cb->motorRunning = (command == WPAD_MOTOR_STOP) ? FALSE : TRUE;
    cb->motorBusy = TRUE;
    OSRestoreInterrupts(enabled);
} // 0x80281060

// Enables or disables rumble for all channels.
void WPADEnableMotor(BOOL enable) {
    BOOL enabled = OSDisableInterrupts();
    _rumble = enable;
    OSRestoreInterrupts(enabled);
} // 0x80281130

// Whether rumble is enabled.
BOOL WPADIsMotorEnabled(void) {
    BOOL enabled = OSDisableInterrupts();
    BOOL rumble = _rumble;
    OSRestoreInterrupts(enabled);
    return rumble;
} // 0x80281170

// Sets the LEDs of a controller (one bit per LED in the high nibble of `flags`).
s32 WPADControlLed(s32 chan, u8 flags, WPADCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL success;
    BOOL enabled;
    BOOL handshake;
    s32 status;

    status = WPAD_ERR_OK;

    enabled = OSDisableInterrupts();
    status = cb->status;
    handshake = cb->handshakeFinished;
    OSRestoreInterrupts(enabled);

    if (status != WPAD_ERR_NO_CONTROLLER) {
        if (!handshake) {
            status = WPAD_ERR_BUSY;
        } else {
            success = WPADiSendSetPort(&cb->stdCmdQueue, flags, callback);
            status = success ? 0 : WPAD_ERR_BUSY;
        }
    }

    if (status != WPAD_ERR_OK && callback != NULL) {
        callback(chan, status);
    }

    return status;
} // 0x802811B0

// Stores the speaker volume and the motor mode in the system configuration.
BOOL WPADSaveConfig(WPADSaveCallback callback) {
    BOOL success = TRUE;
    BOOL enabled;
    u8 volume;
    u8 mode;

    if (SCCheckStatus() != SC_STATUS_OK) {
        return FALSE;
    }

    enabled = OSDisableInterrupts();
    volume = _speakerVolume;
    mode = _rumble ? SC_MOTOR_ON : SC_MOTOR_OFF;
    OSRestoreInterrupts(enabled);

    success &= SCSetWpadSpeakerVolume(volume);
    success &= SCSetWpadMotorMode(mode);

    if (success) {
        SCFlushAsync(callback);
    } else if (callback != NULL) {
        callback(SC_STATUS_FATAL);
    }
    return success;
} // 0x802813A0

// Size of the sample the controller reports in a data format.
static u32 __wpadFmt2Size(u32 format) {
    u32 size;

    switch (format) {
    case WPAD_FMT_FS_BTN:
    case WPAD_FMT_FS_BTN_ACC:
    case WPAD_FMT_FS_BTN_ACC_DPD:
        size = sizeof(WPADFSStatus);
        break;
    case WPAD_FMT_CLASSIC_BTN:
    case WPAD_FMT_CLASSIC_BTN_ACC:
    case WPAD_FMT_CLASSIC_BTN_ACC_DPD:
    case WPAD_FMT_GUITAR:
    case WPAD_FMT_DRUM:
        size = sizeof(WPADCLStatus);
        break;
    case WPAD_FMT_TRAIN:
        size = sizeof(WPADTRStatus);
        break;
    case WPAD_FMT_BALANCE_CHECKER:
        size = sizeof(WPADBLStatus);
        break;
    case WPAD_FMT_VSM:
    case WPAD_FMT_UNK_14:
        size = WPAD_VSM_STATUS_SIZE;
        break;
    case WPAD_FMT_BTN_ACC_DPD_EXTENDED:
        size = sizeof(WPADStatusEx);
        break;
    default:
        size = sizeof(WPADStatus);
        break;
    }
    return size;
}

// Copies the latest sample of a channel.
void WPADRead(s32 chan, WPADStatus* status) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    WPADRxStatus* src;
    u32 size;

    enabled = OSDisableInterrupts();
    src = &cb->rxBufs[(u8)(cb->rxBufIndex == 0)];
    size = __wpadFmt2Size(cb->dataFormat);

    if (src->base.err != WPAD_ERR_OK) {
        size = sizeof(WPADStatus);
    }

    memcpy(status, src, size);
    OSRestoreInterrupts(enabled);
} // 0x80281450

// Installs (or, with NULL, removes) a ring buffer of `len` samples that the library fills with every new report.
void WPADSetAutoSamplingBuf(s32 chan, void* buf, u32 len) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    s8 defaultErr = cb->status == WPAD_ERR_NO_CONTROLLER ? WPAD_ERR_NO_CONTROLLER : WPAD_ERR_INVALID;
    u32 size = __wpadFmt2Size(cb->dataFormat);
    int i;

    if (buf != NULL) {
        memset(buf, 0, size * len);
        for (i = 0; (u32)i < len; i++) {
            ((WPADStatus*)((u8*)buf + i * size))->err = defaultErr;
        }
        cb->samplingBufIndex = -1;
        cb->samplingBufLength = len;
    }
    cb->samplingBuf = buf;
    OSRestoreInterrupts(enabled);
} // 0x80281540

// Drops the second of two opposite directions when both are held (the D-pad of the controller, the Classic
// Controller and the other extensions that have one); the receive buffer holds the sample being parsed.
void WPADiExcludeButton(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    u8 rxBufIndex = cb->rxBufIndex != 0 ? 0 : 1;
    void* rxBuf = &cb->rxBufs[rxBufIndex];
    WPADStatus* status;
    WPADCLStatus* clStatus;
    WPADTRStatus* trStatus;

    status = (WPADStatus*)rxBuf;

    if ((status->button & (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT)) == (WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT)) {
        status->button &= ~WPAD_BUTTON_RIGHT;
    }

    if ((status->button & (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)) == (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)) {
        status->button &= ~WPAD_BUTTON_DOWN;
    }

    if (cb->dataFormat == WPAD_FMT_CLASSIC_BTN || cb->dataFormat == WPAD_FMT_CLASSIC_BTN_ACC
        || cb->dataFormat == WPAD_FMT_CLASSIC_BTN_ACC_DPD || cb->dataFormat == WPAD_FMT_GUITAR
        || cb->dataFormat == WPAD_FMT_DRUM) {
        clStatus = (WPADCLStatus*)rxBuf;

        if ((clStatus->clButton & (WPAD_CL_BUTTON_LEFT | WPAD_CL_BUTTON_RIGHT))
            == (WPAD_CL_BUTTON_LEFT | WPAD_CL_BUTTON_RIGHT)) {
            clStatus->clButton &= ~WPAD_CL_BUTTON_RIGHT;
        }

        if ((clStatus->clButton & (WPAD_CL_BUTTON_UP | WPAD_CL_BUTTON_DOWN))
            == (WPAD_CL_BUTTON_UP | WPAD_CL_BUTTON_DOWN)) {
            clStatus->clButton &= ~WPAD_CL_BUTTON_DOWN;
        }
    }

    if (cb->dataFormat == WPAD_FMT_TRAIN) {
        trStatus = (WPADTRStatus*)status;
        if ((trStatus->trButton & (WPAD_CL_BUTTON_LEFT | WPAD_CL_BUTTON_RIGHT))
            == (WPAD_CL_BUTTON_LEFT | WPAD_CL_BUTTON_RIGHT)) {
            trStatus->trButton &= ~WPAD_CL_BUTTON_RIGHT;
        }
        if ((trStatus->trButton & (WPAD_CL_BUTTON_UP | WPAD_CL_BUTTON_DOWN))
            == (WPAD_CL_BUTTON_UP | WPAD_CL_BUTTON_DOWN)) {
            trStatus->trButton &= ~WPAD_CL_BUTTON_DOWN;
        }
    }
    OSRestoreInterrupts(enabled);
} // 0x802816F0

// Publishes the sample just parsed: copies it into the auto-sampling ring buffer and runs the sampling callback.
void WPADiCopyOut(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    u8 rxBufIndex = cb->rxBufIndex != 0 ? 0 : 1;
    WPADStatus* status = (WPADStatus*)&cb->rxBufs[rxBufIndex];
    u32 size = __wpadFmt2Size(cb->dataFormat);

    if (cb->samplingBuf != NULL) {
        void* dest;

        if (++cb->samplingBufIndex >= cb->samplingBufLength) {
            cb->samplingBufIndex = 0;
        }
        dest = (u8*)cb->samplingBuf + cb->samplingBufIndex * size;
        if (status->err != WPAD_ERR_OK) {
            size = sizeof(WPADStatus);
        }
        memcpy(dest, status, size);
    }
    if (cb->samplingCallback != NULL) {
        cb->samplingCallback(chan);
    }
    cb->packetCnt++;
    OSRestoreInterrupts(enabled);
} // 0x80281810

// Whether the speaker of the controller is powered on.
BOOL WPADIsSpeakerEnabled(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    BOOL speaker = cb->info.speaker;
    OSRestoreInterrupts(enabled);
    return speaker;
} // 0x80281950

// Powers the speaker of a controller on or off, mutes or unmutes it, or starts playback. Switching on queues the
// register writes that configure the speaker (4-bit ADPCM at 3000 Hz), switching off the ones that undo them.
s32 WPADControlSpeaker(s32 chan, u32 command, WPADCallback callback) {
    WPADControlBlock* cb;
    BOOL enabled;
    BOOL speakerOn;
    BOOL handshake;
    s32 status;

    u8 config[SPK_CONFIG_SIZE] = {
        0x00,
        0x00,                            // 4-bit ADPCM
        0xD0, 0x07,                      // 3000 Hz, little-endian
        WPAD_MAX_SPEAKER_VOLUME / 2 + 1, // default volume
        0x0C, 0x0E,
    };

    cb = _wpdcb[chan];

    enabled = OSDisableInterrupts();
    speakerOn = cb->info.speaker;
    status = cb->status;
    handshake = cb->handshakeFinished;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        goto end;
    }

    if (!handshake) {
        status = WPAD_ERR_BUSY;
        goto end;
    }

    if (command == WPAD_SPEAKER_OFF) {
        if (!speakerOn) {
            status = WPAD_ERR_OK;
        } else {
            enabled = OSDisableInterrupts();

            if (__CanPushCmdQueue(&cb->stdCmdQueue, 5)) {
                WPADiSendMuteSpeaker(&cb->stdCmdQueue, TRUE, NULL);
                WPADiSendWriteDataCmd(&cb->stdCmdQueue, 0x01, WM_REG_SPEAKER_01, NULL);
                WPADiSendWriteDataCmd(&cb->stdCmdQueue, 0x00, WM_REG_SPEAKER_09, NULL);
                WPADiSendEnableSpeaker(&cb->stdCmdQueue, FALSE, NULL);
                WPADiSendGetContStat(&cb->stdCmdQueue, NULL, callback);

                OSRestoreInterrupts(enabled);
                return WPAD_ERR_OK;
            }

            status = WPAD_ERR_BUSY;
            OSRestoreInterrupts(enabled);
        }
    } else {
        switch (command) {
        case WPAD_SPEAKER_ON:
        case WPAD_SPEAKER_5:
            enabled = OSDisableInterrupts();

            if (__CanPushCmdQueue(&cb->stdCmdQueue, 7)) {
                WPADiSendEnableSpeaker(&cb->stdCmdQueue, TRUE, NULL);
                WPADiSendMuteSpeaker(&cb->stdCmdQueue, TRUE, NULL);
                WPADiSendWriteDataCmd(&cb->stdCmdQueue, 0x01, WM_REG_SPEAKER_09, NULL);
                WPADiSendWriteDataCmd(&cb->stdCmdQueue, 0x80, WM_REG_SPEAKER_01, NULL);

                config[4] = _speakerVolume;

                WPADiSendWriteData(&cb->stdCmdQueue, config, SPK_CONFIG_SIZE, WM_REG_SPEAKER_01, NULL);
                WPADiSendMuteSpeaker(&cb->stdCmdQueue, FALSE, NULL);
                WPADiSendGetContStat(&cb->stdCmdQueue, NULL, callback);

                OSRestoreInterrupts(enabled);
                return WPAD_ERR_OK;
            }

            status = WPAD_ERR_BUSY;
            OSRestoreInterrupts(enabled);
            break;

        case WPAD_SPEAKER_MUTE:
            if (!WPADiSendMuteSpeaker(&cb->stdCmdQueue, TRUE, callback)) {
                status = WPAD_ERR_BUSY;
                break;
            }
            return WPAD_ERR_OK;

        case WPAD_SPEAKER_UNMUTE:
            if (!WPADiSendMuteSpeaker(&cb->stdCmdQueue, FALSE, callback)) {
                status = WPAD_ERR_BUSY;
                break;
            }
            return WPAD_ERR_OK;

        case WPAD_SPEAKER_PLAY:
            if (!WPADiSendWriteDataCmd(&cb->stdCmdQueue, 0x01, WM_REG_SPEAKER_08, callback)) {
                status = WPAD_ERR_BUSY;
                break;
            }
            return WPAD_ERR_OK;
        }
    }

end:
    if (callback != NULL) {
        callback(chan, status);
    }

    return status;
} // 0x802819A0

// Speaker volume used when the speaker is switched on.
u8 WPADGetSpeakerVolume(void) {
    BOOL enabled = OSDisableInterrupts();
    u8 volume = _speakerVolume;
    OSRestoreInterrupts(enabled);
    return volume;
} // 0x80282FA0

// Sets the speaker volume used when the speaker is switched on.
void WPADSetSpeakerVolume(u8 volume) {
    BOOL enabled = OSDisableInterrupts();
    _speakerVolume = __ClampSpeakerVolume(volume);
    OSRestoreInterrupts(enabled);
} // 0x80282FE0

// Bytes queued for transmission to the channel's controller (0 without a controller).
static u16 __wpadGetBTEBufferStatus(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    s32 status;
    s8 handle;

    enabled = OSDisableInterrupts();
    status = cb->status;
    handle = cb->devHandle;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        return 0;
    }
    return _WUDGetQueuedSize(handle);
}

// Packets sent to the channel's controller but not acknowledged yet (0 without a controller).
static u16 __wpadGetBTMBufferStatus(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    s32 status;
    s8 handle;

    enabled = OSDisableInterrupts();
    status = cb->status;
    handle = cb->devHandle;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        return 0;
    }
    return _WUDGetNotAckedSize(handle);
}

// Whether the Bluetooth stack has no room for another audio frame for the channel.
static BOOL IsBusyStream(s32 chan) {
    BOOL enabled;
    WPADControlBlock* cb = _wpdcb[chan];
    u8 radioQuality;
    u32 devType;
    u8 bufferStatus;
    u16 notAcked;
    u8 audioFrames;
    s8 queueSize;
    u8 linkNumber;

    enabled = OSDisableInterrupts();
    radioQuality = cb->radioQuality;
    devType = cb->devType;
    bufferStatus = WUDGetBufferStatus();
    queueSize = __GetCmdNumber(&cb->stdCmdQueue);
    __wpadGetBTEBufferStatus(chan);
    notAcked = __wpadGetBTMBufferStatus(chan);
    audioFrames = cb->audioFrames;
    linkNumber = _WUDGetLinkNumber();
    OSRestoreInterrupts(enabled);

    if (radioQuality != WPAD_RADIO_QUALITY_GOOD || notAcked > 3 || bufferStatus == 10 ||
        bufferStatus >= linkNumber * 2 + 2 || devType == WPAD_DEV_INITIALIZING ||
        queueSize >= WPAD_COMMAND_CMD_MAX_LEN - 3 || audioFrames >= 1) {
        return TRUE;
    }
    return FALSE;
} // 0x80283030

// Whether the speaker can take another block of sample data.
BOOL WPADCanSendStreamData(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    BOOL setup;
    s32 status;

    enabled = OSDisableInterrupts();
    status = cb->status;
    setup = cb->handshakeFinished;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER || setup == FALSE || IsBusyStream(chan)) {
        return FALSE;
    }
    return TRUE;
} // 0x80283170

// Queues one block of speaker sample data for a controller.
s32 WPADSendStreamData(s32 chan, void* data, u16 len) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    BOOL handshake;
    s32 status;

    enabled = OSDisableInterrupts();
    status = cb->status;
    handshake = cb->handshakeFinished;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        return WPAD_ERR_NO_CONTROLLER;
    }

    if (!handshake) {
        return WPAD_ERR_BUSY;
    }

    if (IsBusyStream(chan)) {
        return WPAD_ERR_BUSY;
    }

    if (!WPADiSendStreamData(&cb->stdCmdQueue, data, len)) {
        return WPAD_ERR_BUSY;
    }

    enabled = OSDisableInterrupts();
    cb->audioFrames++;
    OSRestoreInterrupts(enabled);

    return WPAD_ERR_OK;
} // 0x80283200

// Pointer sensor sensitivity in use (WPAD_MIN_DPD_SENS to WPAD_MAX_DPD_SENS).
u8 WPADGetDpdSensitivity(void) {
    BOOL enabled = OSDisableInterrupts();
    u8 level = _dpdSensitivity;
    OSRestoreInterrupts(enabled);
    return level;
} // 0x80283410

// Whether the pointer sensor of the controller is on.
BOOL WPADIsDpdEnabled(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    BOOL dpd = cb->info.dpd;
    OSRestoreInterrupts(enabled);
    return dpd;
} // 0x80283450

// The pointer sensor format in effect (0 while the sensor is off).
u8 WPADGetDpdFormat(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    u8 command;

    if (cb->info.dpd) {
        command = cb->currentDpdCommand;
    } else {
        command = 0;
    }
    OSRestoreInterrupts(enabled);
    return command;
} // 0x802834A0

// Completion of a pointer sensor (DPD) command: the pending command becomes the current one.
static void __dpdCb(s32 chan, s32 result) {
    WPADControlBlock* cb = _wpdcb[chan];

    cb->currentDpdCommand = cb->pendingDpdCommand;
    cb->dpdBusy = FALSE;
    cb->info.dpd = cb->pendingDpdCommand == WPAD_DPD_DISABLE ? FALSE : TRUE;
} // 0x80283500

// Switches the pointer sensor to one of its data formats (WPAD_DPD_BASIC, ..._STANDARD, ..._EXTENDED) or off. Switching
// on queues the sensor's power-up sequence and the sensitivity blocks for the configured sensitivity.
s32 WPADControlDpd(s32 chan, u32 command, WPADCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    BOOL dpdEnabled;
    BOOL handshake;
    u8 pendingCmd;
    s32 status;

    static const u8 cfg1[WPAD_MAX_DPD_SENS][DPD_CONFIG1_SIZE] = {
        {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x64, 0x00, 254},
        {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0x96, 0x00, 180},
        {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xAA, 0x00, 100},
        {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xC8, 0x00, 54},
        {0x07, 0x00, 0x00, 0x71, 0x01, 0x00, 0x72, 0x00, 32},
    };

    static const u8 cfg2[WPAD_MAX_DPD_SENS][DPD_CONFIG2_SIZE] = {
        {0xFD, 0x05}, {0xB3, 0x04}, {0x63, 0x03}, {0x35, 0x03}, {0x1F, 0x03},
    };

    enabled = OSDisableInterrupts();
    dpdEnabled = cb->info.dpd;
    pendingCmd = cb->pendingDpdCommand;
    status = cb->status;
    handshake = cb->handshakeFinished;
    OSRestoreInterrupts(enabled);

    if (status == WPAD_ERR_NO_CONTROLLER) {
        goto end;
    }

    if (!handshake) {
        status = WPAD_ERR_BUSY;
        goto end;
    }

    if (command == WPAD_DPD_DISABLE) {
        if (!dpdEnabled) {
            status = WPAD_ERR_OK;
        } else {
            enabled = OSDisableInterrupts();

            if (__CanPushCmdQueue(&cb->stdCmdQueue, 3)) {
                cb->pendingDpdCommand = command;

                WPADiSendEnableDPD(&cb->stdCmdQueue, FALSE, NULL);
                WPADiSendDPDCSB(&cb->stdCmdQueue, FALSE, __dpdCb);
                WPADiSendGetContStat(&cb->stdCmdQueue, NULL, callback);

                OSRestoreInterrupts(enabled);
                return WPAD_ERR_OK;
            }

            status = WPAD_ERR_BUSY;
            OSRestoreInterrupts(enabled);
        }
    } else if (command != pendingCmd) {
        enabled = OSDisableInterrupts();

        if (__CanPushCmdQueue(&cb->stdCmdQueue, 8)) {
            cb->pendingDpdCommand = command;
            cb->dpdBusy = TRUE;

            WPADiSendEnableDPD(&cb->stdCmdQueue, TRUE, NULL);
            WPADiSendDPDCSB(&cb->stdCmdQueue, TRUE, NULL);
            WPADiSendWriteDataCmd(&cb->stdCmdQueue, 0x01, WM_REG_DPD_30, NULL);
            WPADiSendWriteData(&cb->stdCmdQueue, (void*)cfg1[_dpdSensitivity - 1], DPD_CONFIG1_SIZE,
                               WM_REG_DPD_CONFIG_BLOCK_1, NULL);
            WPADiSendWriteData(&cb->stdCmdQueue, (void*)cfg2[_dpdSensitivity - 1], DPD_CONFIG2_SIZE,
                               WM_REG_DPD_CONFIG_BLOCK_2, NULL);
            WPADiSendWriteDataCmd(&cb->stdCmdQueue, command, WM_REG_DPD_DATA_FORMAT, NULL);
            WPADiSendWriteDataCmd(&cb->stdCmdQueue, 0x08, WM_REG_DPD_30, __dpdCb);
            WPADiSendGetContStat(&cb->stdCmdQueue, NULL, callback);

            OSRestoreInterrupts(enabled);
            return WPAD_ERR_OK;
        }

        status = WPAD_ERR_BUSY;
        OSRestoreInterrupts(enabled);
    }

end:
    if (callback != NULL) {
        callback(chan, status);
    }

    return status;
} // 0x80283540

// Balance Board battery-level control: writes the sequence that switches the board's battery monitoring on (0xAA),
// off (0x55) or clears it (0) to the board's register 0xF1.
s32 WPADControlBLC(s32 chan, s32 command, WPADCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled = OSDisableInterrupts();
    s32 status = cb->status;
    BOOL handshake = cb->handshakeFinished;

    OSRestoreInterrupts(enabled);
    if (status == WPAD_ERR_NO_CONTROLLER) {
        goto end;
    }
    if (!handshake || !WUDIsLinkedWBC()) {
        status = WPAD_ERR_BUSY;
        goto end;
    } else {
        u8 data[7];

        data[0] = data[1] = data[2] = 0xAA;
        data[3] = 0x55;
        data[4] = data[5] = data[6] = command;
        enabled = OSDisableInterrupts();
        switch (command) {
        case 0xAA:
            if (__CanPushCmdQueue(&cb->stdCmdQueue, 4)) {
                WPADWriteExtReg(chan, data, sizeof(data), 0xF1, NULL);
                WPADWriteExtReg(chan, data, 1, 0xF1, NULL);
                WPADWriteExtReg(chan, data, 1, 0xF1, NULL);
                WPADWriteExtReg(chan, data, 1, 0xF1, callback);
                OSRestoreInterrupts(enabled);
                return WPAD_ERR_OK;
            }
            status = WPAD_ERR_BUSY;
            break;
        case 0x55:
            status = WPADWriteExtReg(chan, data, sizeof(data), 0xF1, callback);
            if (status == WPAD_ERR_OK) {
                OSRestoreInterrupts(enabled);
                return WPAD_ERR_OK;
            }
            break;
        case 0:
            status = WPADWriteExtReg(chan, data, 1, 0xF1, callback);
            if (status == WPAD_ERR_OK) {
                OSRestoreInterrupts(enabled);
                return WPAD_ERR_OK;
            }
            break;
        default:
            status = WPAD_ERR_BUSY;
            break;
        }
    }
    OSRestoreInterrupts(enabled);
end:
    if (callback != NULL) {
        callback(chan, status);
    }
    return status;
} // 0x802845C0

// Queues the output report that selects the input report format: the report id that carries the data a data format
// consists of, and whether the controller reports continuously.
BOOL WPADiSendSetReportType(WPADCmdQueue* queue, u32 format, u8 savePower, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_SET_DATA_REPORT_MODE;
    command.length = RPT12_SIZE;
    command.data[0] = savePower ? 0 : 4;
    command.callback = callback;

    switch (format) {
    case WPAD_FMT_CORE_BTN:
        command.data[1] = RPTID_DATA_BTN;
        break;
    case WPAD_FMT_CORE_BTN_ACC:
        command.data[1] = RPTID_DATA_BTN_ACC;
        break;
    case WPAD_FMT_CORE_BTN_ACC_DPD:
        command.data[1] = RPTID_DATA_BTN_ACC_DPD12;
        break;
    case WPAD_FMT_FS_BTN:
        command.data[1] = RPTID_DATA_BTN_EXT8;
        break;
    case WPAD_FMT_FS_BTN_ACC:
        command.data[1] = RPTID_DATA_BTN_ACC_EXT16;
        break;
    case WPAD_FMT_FS_BTN_ACC_DPD:
        command.data[1] = RPTID_DATA_BTN_ACC_DPD10_EXT9;
        break;
    case WPAD_FMT_CLASSIC_BTN:
        command.data[1] = RPTID_DATA_BTN_EXT8;
        break;
    case WPAD_FMT_CLASSIC_BTN_ACC:
        command.data[1] = RPTID_DATA_BTN_ACC_EXT16;
        break;
    case WPAD_FMT_CLASSIC_BTN_ACC_DPD:
        command.data[1] = RPTID_DATA_BTN_ACC_DPD10_EXT9;
        break;
    case WPAD_FMT_BTN_ACC_DPD_EXTENDED:
        command.data[1] = RPTID_DATA_BTN_ACC_DPD18_1;
        break;
    case WPAD_FMT_TRAIN:
        command.data[1] = RPTID_DATA_BTN_EXT8;
        break;
    case WPAD_FMT_GUITAR:
        command.data[1] = RPTID_DATA_BTN_ACC_DPD10_EXT9;
        break;
    case WPAD_FMT_DRUM:
        command.data[1] = RPTID_DATA_BTN_ACC_DPD10_EXT9;
        break;
    case WPAD_FMT_BALANCE_CHECKER:
        command.data[1] = RPTID_DATA_BTN_EXT19;
        break;
    case WPAD_FMT_VSM:
        command.data[1] = RPTID_DATA_BTN_ACC_EXT16;
        break;
    case WPAD_FMT_UNK_14:
        command.data[1] = RPTID_DATA_EXT21;
        break;
    }

    return WPADiPushCommand(queue, command);
} // 0x802847D0

// Queues a write of one byte to a controller address.
BOOL WPADiSendWriteDataCmd(WPADCmdQueue* queue, u8 cmd, u32 addr, WPADCallback callback) {
    return WPADiSendWriteData(queue, &cmd, sizeof(u8), addr, callback);
} // 0x80284A40

// Queues a write of `len` bytes (up to 16) to a controller address.
BOOL WPADiSendWriteData(WPADCmdQueue* queue, void* buf, u16 len, u32 addr, WPADCallback callback) {
    WPADCommand command;
    u8 packedLen = len & 31;

    command.reportId = RPTID_WRITE_DATA;
    command.length = RPT16_SIZE;
    command.callback = callback;
    memcpy(&command.data[RPT16_DATA_DST_ADDRESS], &addr, sizeof(u32));
    memcpy(&command.data[RPT16_DATA_LENGTH], &packedLen, sizeof(u8));
    memcpy(&command.data[RPT16_DATA], buf, len);
    return WPADiPushCommand(queue, command);
} // 0x80284C00

// Queues a read of `len` bytes from a controller address into `dst`.
BOOL WPADiSendReadData(WPADCmdQueue* queue, void* dst, u16 len, u32 addr, WPADCallback callback) {
    WPADCommand command;

    command.reportId = RPTID_READ_DATA;
    command.length = RPT17_SIZE;
    command.callback = callback;
    memcpy(&command.data[RPT17_DATA_SRC_ADDRESS], &addr, sizeof(u32));
    memcpy(&command.data[RPT17_DATA_LENGTH], &len, sizeof(u16));
    command.readBuf = dst;
    command.readLength = len;
    command.readAddress = addr;
    return WPADiPushCommand(queue, command);
} // 0x80284DC0

// Empties a command queue.
void WPADiClearQueue(WPADCmdQueue* queue) {
    BOOL enabled = OSDisableInterrupts();

    queue->head = 0;
    queue->tail = 0;
    memset(queue->commands, 0, queue->capacity * sizeof(WPADCommand));
    OSRestoreInterrupts(enabled);
} // 0x80284F70

void WPADSetCallbackByKPAD(BOOL isKPAD) {
    _callbackByKPAD = isKPAD;
} // 0x80284FD0
