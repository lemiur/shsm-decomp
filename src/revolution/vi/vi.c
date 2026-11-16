// Reference: zeldaret/tp libs/revolution/src/vi/vi.c
//
// Retail keeps only the functions the game links; the linker dead-stripped the rest of the SDK object. Absent from
// the DOL (not even inlined): the latch and raw-position interrupt functions, __VISetExtraTiming, the adjusting-value
// getters and setters, VISet3D, VISetNextRightFrameBuffer, VIGetNextFrameBuffer, VIGetCurrentFrameBuffer and the debug
// assertions. Present here with no address, as in the reference, because retail inlines them (VIEnableDimming,
// VIEnableDVDStopMotor, __VIResetDev0Idle, __VIGetCurrentPosition) or because they were stripped
// (VIResetRFIdle, __VIResetSIIdle); their data references decide where the file-scope statics land.
#include <revolution/gx.h>
#include <revolution/os.h>
#include <revolution/sc.h>
#include <revolution/si.h>
#include <revolution/vi.h>

#include <revolution/gx/__gx.h>
#include <revolution/vi/__vi.h>
#include <revolution/dvd/__dvd.h>

#define BUILD_DATE "Feb 27 2009"
#define BUILD_TIME "10:04:46"

/*
 * Idle retraces before the screen saver dims the picture (5 minutes by default, 10 or 15 minutes selectable)
 * and before the DVD motor is stopped (30 minutes), for 60Hz (NTSC, MPAL, EURGB60) and 50Hz (PAL) video.
 * The macro names down to VI_FBB_SHIFT_MIN are invented: the SDK's own names for these values are unknown.
 */
#define VI_DIM_5M_60HZ    18000
#define VI_DIM_10M_60HZ   36000
#define VI_DIM_15M_60HZ   54000
#define VI_DIM_5M_50HZ    15000
#define VI_DIM_10M_50HZ   30000
#define VI_DIM_15M_50HZ   45000
#define VI_DVD_STOP_60HZ  108000
#define VI_DVD_STOP_50HZ  90000

/* Interrupt flag of the display interrupt registers (cleared by writing 0) and the neighbouring enable bit. */
#define VI_DI_INTERRUPT   0x8000
#define VI_DI_ENABLE      0x1000

/* Beam position registers, indexed by halfword. */
#define VI_VERT_COUNT     0x16
#define VI_HORIZ_COUNT    0x17

#define U32_MAX           0xFFFFFFFF

/* Frame buffer addresses are physical and 32-byte aligned; from 16MB up the registers hold them divided by 32. */
#define VI_FBB_PHYS_MASK  0x3FFFFFFF
#define VI_FBB_SHIFT_MIN  0x01000000U

const char* __VIVersion = "<< RVL_SDK - VI \trelease build: " BUILD_DATE " " BUILD_TIME " (0x4302_145) >>";

/* One row of the video timing table: field counts and sync/blanking intervals of one TV mode. */
typedef struct {
    u8 equ;
    u16 acv;
    u16 prbOdd;
    u16 prbEven;
    u16 psbOdd;
    u16 psbEven;
    u8 bs1;
    u8 bs2;
    u8 bs3;
    u8 bs4;
    u16 be1;
    u16 be2;
    u16 be3;
    u16 be4;
    u16 nhlines;
    u16 hlw;
    u8 hsy;
    u8 hcs;
    u8 hce;
    u8 hbe640;
    u16 hbs640;
    u8 hbeCCIR656;
    u16 hbsCCIR656;
} timing_s;

/* Current picture geometry and frame buffer state of the VI (name invented; the reference calls it SomeVIStruct). */
typedef struct {
    u16 DispPosX;
    u16 DispPosY;
    u16 DispSizeX;
    u16 DispSizeY;
    u16 AdjustedDispPosX;
    u16 AdjustedDispPosY;
    u16 AdjustedDispSizeY;
    u16 AdjustedPanPosY;
    u16 AdjustedPanSizeY;
    u16 FBSizeX;
    u16 FBSizeY;
    u16 PanPosX;
    u16 PanPosY;
    u16 PanSizeX;
    u16 PanSizeY;
    VIXFBMode FBMode;
    u32 nonInter;
    u32 tv;
    u8 wordPerLine;
    u8 std;
    u8 wpl;
    u32 bufAddr;
    u32 tfbb;
    u32 bfbb;
    u8 xof;
    BOOL black;
    BOOL threeD;
    u32 rbufAddr;
    u32 rtfbb;
    u32 rbfbb;
    timing_s* timing;
} VIDisplayState;

static BOOL IsInitialized = FALSE;

static volatile u32 retraceCount; // volatile: shared with the retrace interrupt handler
static volatile u32 flushFlag; // volatile: shared with the retrace interrupt handler
static volatile u32 flushFlag3in1; // volatile: shared with the retrace interrupt handler
static volatile u32 vsync_timing_err_cnt = 0; // volatile: shared with the retrace interrupt handler
static volatile u32 vsync_timing_test_flag = 0; // volatile: shared with the retrace interrupt handler

static volatile BOOL __VIDimming_All_Clear = FALSE; // volatile: shared with the retrace interrupt handler
static volatile BOOL __VIDimmingFlag_Enable; // volatile: shared with the retrace interrupt handler
static volatile BOOL __VIDVDStopFlag_Enable; // volatile: shared with the retrace interrupt handler
static volatile VITimeToDIM g_current_time_to_dim; // volatile: shared with the retrace interrupt handler
static vu32 THD_TIME_TO_DIMMING = 0; // volatile: shared with the retrace interrupt handler
static vu32 NEW_TIME_TO_DIMMING = 0; // volatile: shared with the retrace interrupt handler
static vu32 THD_TIME_TO_DVD_STOP = 0; // volatile: shared with the retrace interrupt handler
static vu32 _gIdleCount_dimming = 0; // volatile: shared with the retrace interrupt handler
static vu32 _gIdleCount_dvd = 0; // volatile: shared with the retrace interrupt handler
static vu32 __VIDimmingFlag_RF_IDLE; // volatile: shared with the retrace interrupt handler
static vu32 __VIDimmingFlag_SI_IDLE; // volatile: shared with the retrace interrupt handler
static vu32 __VIDimmingFlag_DEV_IDLE[10]; // volatile: shared with the retrace interrupt handler
static volatile BOOL __VIDimmingState = FALSE; // volatile: shared with the retrace interrupt handler

static OSThreadQueue retraceQueue;

static void (*PreCB)(u32);
static void (*PostCB)(u32);

static u32 encoderType;
static u16 regs[59];
static timing_s* CurrTiming;
static u32 CurrTvMode;
static u32 NextBufAddr;
static u32 CurrBufAddr;
static u16 shdwRegs[59];

static void (*PositionCallback)(s16, s16) = NULL;
static s16 displayOffsetH = 0;
static s16 displayOffsetV = 0;
static volatile u32 changeMode = 0; // volatile: VIFlush (game threads) and the retrace interrupt handler both update it
static volatile u64 changed = 0; // volatile: VIFlush (game threads) and the retrace interrupt handler both update it
static volatile u32 shdwChangeMode = 0; // volatile: VIFlush (game threads) and the retrace interrupt handler both update it
static volatile u64 shdwChanged = 0; // volatile: VIFlush (game threads) and the retrace interrupt handler both update it
static u32 FBSet = 0;
static timing_s* timingExtra = NULL;

#define MARK_CHANGED(index) (changed |= 1LL << (63 - (index)))

static timing_s timing[11] = {
    { 6, 240, 24, 25, 3, 2, 12, 13, 12, 13, 520, 519, 520, 519, 525, 429, 64, 71, 105, 162, 373, 122, 412 },
    { 6, 240, 24, 24, 4, 4, 12, 12, 12, 12, 520, 520, 520, 520, 526, 429, 64, 71, 105, 162, 373, 122, 412 },
    { 5, 287, 35, 36, 1, 0, 13, 12, 11, 10, 619, 618, 617, 620, 625, 432, 64, 75, 106, 172, 380, 133, 420 },
    { 5, 287, 33, 33, 2, 2, 13, 11, 13, 11, 619, 621, 619, 621, 624, 432, 64, 75, 106, 172, 380, 133, 420 },
    { 6, 240, 24, 25, 3, 2, 16, 15, 14, 13, 518, 517, 516, 519, 525, 429, 64, 78, 112, 162, 373, 122, 412 },
    { 6, 240, 24, 24, 4, 4, 16, 14, 16, 14, 518, 520, 518, 520, 526, 429, 64, 78, 112, 162, 373, 122, 412 },
    { 12, 480, 48, 48, 6, 6, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 162, 373, 122, 412 },
    { 12, 480, 44, 44, 10, 10, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 168, 379, 122, 412 },
    { 6, 241, 24, 25, 1, 0, 12, 13, 12, 13, 520, 519, 520, 519, 525, 429, 64, 71, 105, 159, 370, 122, 412 },
    { 12, 480, 48, 48, 6, 6, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 180, 391, 122, 412 },
    { 10, 576, 62, 62, 6, 6, 20, 20, 20, 20, 1240, 1240, 1240, 1240, 1250, 432, 64, 75, 106, 172, 380, 122, 412 }
};

static u16 taps[25] = {
    0x01F0, 0x01DC,
    0x01AE, 0x0174,
    0x0129, 0x00DB,
    0x008E, 0x0046,
    0x000C, 0x00E2,
    0x00CB, 0x00C0,
    0x00C4, 0x00CF,
    0x00DE, 0x00EC,
    0x00FC, 0x0008,
    0x000F, 0x0013,
    0x0013, 0x000F,
    0x000C, 0x0008,
    0x0001
};

GXRenderModeObj GXPal528Prog = {
    6,
    640, 528, 528,
    40, 23,
    640, 528,
    0,
    0,
    0,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXPal528ProgSoft = {
    6,
    640, 528, 528,
    40, 23,
    640, 528,
    0,
    0,
    0,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXPal524ProgAa = {
    6,
    640, 264, 524,
    40, 23,
    640, 524,
    0,
    0,
    1,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

static BOOL OnShutdown(BOOL final, u32 event);
static OSShutdownFunctionInfo ShutdownFunctionInfo = {
    OnShutdown,
    127
};

static VIDisplayState HorVer;

static u32 getCurrentFieldEvenOdd(void);
static void GetCurrentDisplayPosition(u32* hct, u32* vct);

/*
 * OnShutdown
 *
 * Shutdown function (priority 127): on the first call of a reset it forces RGB mode and flushes, then reports
 * ready once a retrace has passed; for the final power-off events it restores gamma 1.0.
 */
static BOOL OnShutdown(BOOL final, u32 event) {
    BOOL retval;
    static BOOL first = TRUE;
    static u32 count;

    if (final == FALSE) {
        switch (event) {
        case 3:
        case 1:
        case 2:
            if (first) {
                VISetRGBModeImm();
                VIFlush();
                count = retraceCount;
                first = FALSE;
                retval = FALSE;
            } else {
                if (count == retraceCount) {
                    retval = FALSE;
                } else {
                    retval = TRUE;
                }
            }
            break;
        case 4:
        case 0:
        case 6:
        case 5:
            __VISetGamma1_0();
            retval = TRUE;
            break;
        }
    } else {
        retval = TRUE;
    }

    return retval;
} // 0x80227F50

/*
 * cntlzd
 *
 * Counts the leading zeros of a 64-bit value (the VI register bitmaps are most-significant-bit first).
 */
static s32 cntlzd(u64 bit) {
    u32 hi;
    u32 lo;
    s32 value;

    hi = bit >> 32;
    lo = bit & U32_MAX;
    value = __cntlzw(hi);
    if (value < 32) {
        return value;
    }
    return __cntlzw(lo) + 32;
}

/*
 * VISetRegs
 *
 * Copies the flushed shadow registers to the hardware. A change of scan mode waits for an even field.
 */
static int VISetRegs(void) {
    s32 regIndex;

    if (shdwChangeMode != 1 || getCurrentFieldEvenOdd() != 0) {
        while (shdwChanged != 0) {
            regIndex = cntlzd(shdwChanged);
            __VIRegs[regIndex] = shdwRegs[regIndex];
            shdwChanged &= ~((u64)1 << (63 - regIndex));
        }

        shdwChangeMode = 0;
        CurrTiming = HorVer.timing;
        CurrTvMode = HorVer.tv;
        CurrBufAddr = NextBufAddr;
        return 1;
    }

    return 0;
}

/*
 * __VIRetraceHandler
 *
 * Retrace interrupt handler: position interrupts call the position callback; a retrace applies a pending
 * VIFlush, tracks DTV/TV format changes, runs the pre/post callbacks and the dimming/DVD-stop idle counters.
 */
static void __VIRetraceHandler(__OSInterrupt unused, OSContext* context) {
    OSContext exceptionContext;
    u16 reg;
    u32 inter = 0;
    u32 regIndex;
    static u32 old_dtvStatus = 999;
    static u32 old_tvtype = 999;
    u32 now_dtvStatus = 0;
    u32 now_tvtype = 0;
    static BOOL __VIDimmingFlag_Enable_old = TRUE;
    static BOOL __VIDVDStopFlag_Enable_old = TRUE;
    u32 i;
    static u32 DimmingON_Pending = 0;
    static u32 DimmingOFF_Pending = 0;

    reg = __VIRegs[0x18];
    if (reg & VI_DI_INTERRUPT) {
        __VIRegs[0x18] = reg & ~VI_DI_INTERRUPT;
        inter |= 1;
    }
    reg = __VIRegs[0x1A];
    if (reg & VI_DI_INTERRUPT) {
        __VIRegs[0x1A] = reg & ~VI_DI_INTERRUPT;
        inter |= 2;
    }
    reg = __VIRegs[0x1C];
    if (reg & VI_DI_INTERRUPT) {
        __VIRegs[0x1C] = reg & ~VI_DI_INTERRUPT;
        inter |= 4;
    }
    reg = __VIRegs[0x1E];
    if (reg & VI_DI_INTERRUPT) {
        __VIRegs[0x1E] = reg & ~VI_DI_INTERRUPT;
        inter |= 8;
    }
    reg = __VIRegs[0x1E];

    if ((inter & 4) || (inter & 8)) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);

        if (PositionCallback != 0) {
            s16 x, y;
            __VIGetCurrentPosition(&x, &y);
            (*PositionCallback)(x, y);
        }

        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
        return;
    }

    retraceCount += 1;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);

    if (PreCB) {
        PreCB(retraceCount);
    }

    if (vsync_timing_test_flag) {
        u32 hcount, vcount;

        GetCurrentDisplayPosition(&hcount, &vcount);

        if (!((vcount == 1) || (vcount == (CurrTiming->nhlines / 2 + 1)))) {
            vsync_timing_err_cnt++;
        }
    }

    if (flushFlag != 0) {
        if (VISetRegs() != 0) {
            flushFlag = 0;
            SIRefreshSamplingRate();
        }
    }

    now_dtvStatus = VIGetDTVStatus();
    if (now_dtvStatus != old_dtvStatus) {
        __VISetYUVSEL((VIBool)now_dtvStatus);
    }

    old_dtvStatus = now_dtvStatus;
    now_tvtype = VIGetTvFormat();

    if (now_tvtype != old_tvtype) {
        if (now_tvtype == VI_EURGB60) {
            __VISetFilter4EURGB60(VI_ENABLE);
        } else {
            __VISetFilter4EURGB60(VI_DISABLE);
        }

        switch (now_tvtype) {
        case VI_PAL:
            switch (g_current_time_to_dim) {
            case VI_DM_10M:
                NEW_TIME_TO_DIMMING = VI_DIM_10M_50HZ;
                break;
            case VI_DM_15M:
                NEW_TIME_TO_DIMMING = VI_DIM_15M_50HZ;
                break;
            default:
                NEW_TIME_TO_DIMMING = VI_DIM_5M_50HZ;
                break;
            }
            THD_TIME_TO_DVD_STOP = VI_DVD_STOP_50HZ;
            break;
        default:
            switch (g_current_time_to_dim) {
            case VI_DM_10M:
                NEW_TIME_TO_DIMMING = VI_DIM_10M_60HZ;
                break;
            case VI_DM_15M:
                NEW_TIME_TO_DIMMING = VI_DIM_15M_60HZ;
                break;
            default:
                NEW_TIME_TO_DIMMING = VI_DIM_5M_60HZ;
                break;
            }
            THD_TIME_TO_DVD_STOP = VI_DVD_STOP_60HZ;
            break;
        }

        _gIdleCount_dimming = 0;
        _gIdleCount_dvd = 0;
    }

    old_tvtype = now_tvtype;

    if (flushFlag3in1) {
        while (Vdac_Flag_Changed) {
            regIndex = (u32)__cntlzw(Vdac_Flag_Changed);
            regIndex = (u32)(1 << (31 - regIndex));

            switch (regIndex) {
            case 1:
                __VISetCGMS();
                break;
            case 2:
                __VISetWSS();
                break;
            case 4:
                __VISetClosedCaption();
                break;
            case 8:
                __VISetMacrovision();
                break;
            case 0x10:
                __VISetGamma();
                break;
            case 0x20:
                __VISetTrapFilter();
                break;
            case 0x40:
                __VISetRGBOverDrive();
                break;
            case 0x80:
                __VISetRGBModeImm();
                break;
            }

            Vdac_Flag_Changed &= ~regIndex;
        }

        flushFlag3in1 = 0;
    }

    if (PostCB) {
        OSClearContext(&exceptionContext);
        PostCB(retraceCount);
    }

    OSWakeupThread(&retraceQueue);
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);

    if (__VIDimming_All_Clear == TRUE) {
        if (__OSSetVIForceDimming(FALSE, 0, 0) == TRUE) {
            __VIDimming_All_Clear = FALSE;
            _gIdleCount_dimming = 0;
        }
    }

    for (i = 0; i < 10; i++) {
        if (__VIDimmingFlag_DEV_IDLE[i] == 0) {
            __VIDimmingFlag_DEV_IDLE[0] = 0;
            break;
        }
    }

    if (__VIDimmingFlag_RF_IDLE && __VIDimmingFlag_SI_IDLE && __VIDimmingFlag_DEV_IDLE[0]) {
        if ((__VIDimmingFlag_Enable == TRUE) && (_gIdleCount_dimming < U32_MAX)) {
            _gIdleCount_dimming++;
        }

        if ((__VIDVDStopFlag_Enable == TRUE) && (_gIdleCount_dvd < U32_MAX)) {
            _gIdleCount_dvd++;
        }
    } else {
        if (_gIdleCount_dimming >= THD_TIME_TO_DIMMING) {
            DimmingOFF_Pending = 1;
        }
        if (_gIdleCount_dvd >= THD_TIME_TO_DVD_STOP) {
            __DVDRestartMotor();
        }

        _gIdleCount_dimming = 0;
        _gIdleCount_dvd = 0;
        THD_TIME_TO_DIMMING = NEW_TIME_TO_DIMMING;
    }

    if (__VIDimmingFlag_Enable_old != __VIDimmingFlag_Enable) {
        if (__VIDimmingFlag_Enable == FALSE) {
            if (_gIdleCount_dimming >= THD_TIME_TO_DIMMING) {
                DimmingOFF_Pending = 1;
            }
        }

        _gIdleCount_dimming = 0;
        THD_TIME_TO_DIMMING = NEW_TIME_TO_DIMMING;
    }

    if (_gIdleCount_dimming == THD_TIME_TO_DIMMING) {
        DimmingON_Pending = 1;
    }

    if (DimmingOFF_Pending) {
        if (__OSSetVIForceDimming(FALSE, 2, 2) == TRUE) {
            DimmingOFF_Pending = 0;
            __VIDimmingState = FALSE;
        }
    }

    if (DimmingON_Pending) {
        if (__OSSetVIForceDimming(TRUE, 2, 2) == TRUE) {
            DimmingON_Pending = 0;
            __VIDimmingState = TRUE;
        }
    }

    if (__VIDVDStopFlag_Enable_old != __VIDVDStopFlag_Enable) {
        if (__VIDVDStopFlag_Enable == FALSE) {
            if (_gIdleCount_dvd >= THD_TIME_TO_DVD_STOP) {
                __DVDRestartMotor();
            }
        }
        _gIdleCount_dvd = 0;
    }

    if (_gIdleCount_dvd == THD_TIME_TO_DVD_STOP) {
        __DVDStopMotorAsync(&__DVDStopMotorCommandBlock, NULL);
    }

    __VIDimmingFlag_RF_IDLE = 1;
    __VIDimmingFlag_SI_IDLE = 1;

    for (i = 0; i < 10; i++) {
        __VIDimmingFlag_DEV_IDLE[i] = 1;
    }

    __VIDimmingFlag_Enable_old = __VIDimmingFlag_Enable;
    __VIDVDStopFlag_Enable_old = __VIDVDStopFlag_Enable;

    if ((NEW_TIME_TO_DIMMING > _gIdleCount_dimming) && (__VIDimmingState == FALSE)) {
        THD_TIME_TO_DIMMING = NEW_TIME_TO_DIMMING;
    }
} // 0x802280F0

/*
 * VISetPreRetraceCallback
 *
 * Sets the callback run at the start of each retrace; returns the previous one.
 */
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb) {
    BOOL enabled;
    VIRetraceCallback oldcb;

    oldcb = PreCB;
    enabled = OSDisableInterrupts();
    PreCB = cb;
    OSRestoreInterrupts(enabled);
    return oldcb;
} // 0x80228900

/*
 * VISetPostRetraceCallback
 *
 * Sets the callback run at the end of each retrace; returns the previous one.
 */
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb) {
    BOOL enabled;
    VIRetraceCallback oldcb;

    oldcb = PostCB;
    enabled = OSDisableInterrupts();
    PostCB = cb;
    OSRestoreInterrupts(enabled);
    return oldcb;
} // 0x80228950

#pragma dont_inline on
/*
 * getTiming
 *
 * Timing table row for a TV mode code (the extra modes use the row set by __VISetExtraTiming).
 */
static timing_s* getTiming(VITVMode mode) {
    switch (mode) {
    case VI_TVMODE_NTSC_INT:        return &timing[0];
    case VI_TVMODE_NTSC_DS:         return &timing[1];
    case VI_TVMODE_PAL_INT:         return &timing[2];
    case VI_TVMODE_PAL_DS:          return &timing[3];
    case VI_TVMODE_EURGB60_INT:     return &timing[0];
    case VI_TVMODE_EURGB60_DS:      return &timing[1];
    case VI_TVMODE_MPAL_INT:        return &timing[4];
    case VI_TVMODE_MPAL_DS:         return &timing[5];
    case VI_TVMODE_NTSC_PROG:
    case VI_TVMODE_MPAL_PROG:
    case VI_TVMODE_EURGB60_PROG:    return &timing[6];
    case VI_TVMODE_NTSC_3D:         return &timing[7];
    case VI_TVMODE_DEBUG_PAL_INT:   return &timing[2];
    case VI_TVMODE_DEBUG_PAL_DS:    return &timing[3];
    case VI_TVMODE_GCA_INT:         return &timing[8];
    case VI_TVMODE_GCA_PROG:        return &timing[9];
    case VI_TVMODE_PAL_PROG:        return &timing[10];
    case VI_TVMODE_EXTRA_INT:
    case VI_TVMODE_EXTRA_DS:
    case VI_TVMODE_EXTRA_PROG:
    case VI_TVMODE_HD720_PROG:      return timingExtra;
    default:
        return NULL;
    }
} // 0x802289A0
#pragma dont_inline reset

/*
 * __VIInit
 *
 * Programs the VI registers for the boot TV mode and starts the output.
 */
void __VIInit(VITVMode mode) {
    timing_s* tm;
    u32 nonInter;
    u32 tv;
    u32 tvForReg;
    volatile u32 a; // volatile: delay loop, zeldaret/tp libs/revolution/src/vi/vi.c (__VIInit)
    u16 hct;
    u16 vct;

    nonInter = mode & 3;
    tv = (u32)mode >> 2;
    *(u32*)OSPhysicalToCached(0xCC) = tv;

    tm = getTiming(mode);
    __VIRegs[1] = 2;

    for (a = 0; a < 1000; a++) {}

    __VIRegs[1] = 0;
    __VIRegs[3] = (u32)tm->hlw;
    __VIRegs[2] = tm->hce | (tm->hcs << 8);
    __VIRegs[5] = tm->hsy | ((tm->hbe640 & 0x1FF) << 7);
    __VIRegs[4] = (tm->hbe640 >> 9) | (tm->hbs640 << 1);

    if (encoderType == 0) {
        __VIRegs[0x39] = tm->hbeCCIR656 | 0x8000;
        __VIRegs[0x3A] = (u32)tm->hbsCCIR656;
    }

    __VIRegs[0] = (u32)tm->equ;
    __VIRegs[7] = (u32)(tm->prbOdd + (tm->acv * 2) - 2);
    __VIRegs[6] = (u32)(tm->psbOdd + 2);
    __VIRegs[9] = (u32)(tm->prbEven + (tm->acv * 2) - 2);
    __VIRegs[8] = (u32)(tm->psbEven + 2);
    __VIRegs[11] = tm->bs1 | (tm->be1 << 5);
    __VIRegs[10] = tm->bs3 | (tm->be3 << 5);
    __VIRegs[13] = tm->bs2 | (tm->be2 << 5);
    __VIRegs[12] = tm->bs4 | (tm->be4 << 5);
    __VIRegs[36] = 0x2828;
    __VIRegs[27] = 1;
    __VIRegs[26] = 0x1001;
    hct = tm->hlw + 1;
    vct = (tm->nhlines / 2) + 1;
    __VIRegs[25] = (u16)(u32)hct;
    __VIRegs[24] = vct | VI_DI_ENABLE;

    switch (tv) {
    case VI_PAL:
    case VI_MPAL:
    case VI_DEBUG:
        tvForReg = tv;
        break;
    default:
        tvForReg = 0;
    }

    if (nonInter == 0 || nonInter == 1) {
        __VIRegs[1] = ((nonInter & 1) << 2) | 1 | (tvForReg << 8);
        __VIRegs[54] = 0;
        return;
    }

    __VIRegs[1] = (tvForReg << 8) | 5;
    __VIRegs[54] = 1;
} // 0x80228AC0

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(val, min, max) ((val) > (max) ? (max) : (val) < (min) ? (min) : (val))

/*
 * AdjustPosition
 *
 * Clamps the display origin to the screen after the user display offset and derives the adjusted pan.
 */
static void AdjustPosition(u16 acv) {
    s32 coeff;
    s32 frac;

    HorVer.AdjustedDispPosX = CLAMP((s16)HorVer.DispPosX + displayOffsetH, 0, 0x2D0 - HorVer.DispSizeX);
    coeff = (HorVer.FBMode == VI_XFBMODE_SF) ? 2 : 1;
    frac = HorVer.DispPosY & 1;
    HorVer.AdjustedDispPosY = MAX((s16)HorVer.DispPosY + displayOffsetV, frac);
    HorVer.AdjustedDispSizeY = HorVer.DispSizeY
                             + MIN((s16)HorVer.DispPosY + displayOffsetV - frac, 0)
                             - MAX((s16)HorVer.DispPosY + (s16)HorVer.DispSizeY + displayOffsetV - (((s16)acv * 2) - frac), 0);
    HorVer.AdjustedPanPosY = HorVer.PanPosY
                           - (MIN((s16)HorVer.DispPosY + displayOffsetV - frac, 0) / coeff);
    HorVer.AdjustedPanSizeY = HorVer.PanSizeY
                            + (MIN((s16)HorVer.DispPosY + displayOffsetV - frac, 0) / coeff)
                            - (MAX((s16)HorVer.DispPosY + (s16)HorVer.DispSizeY + displayOffsetV - (((s16)acv * 2) - frac), 0) / coeff);
}

/*
 * ImportAdjustingValues
 *
 * Loads the horizontal display offset from the system settings (the vertical offset starts at 0).
 */
static void ImportAdjustingValues(void) {
    displayOffsetH = SCGetDisplayOffsetH();
    displayOffsetV = 0;
}

/*
 * VIInit
 *
 * Initialises the video interface: filter taps, the retrace interrupt, the shutdown function and the
 * screen-saver (dimming) and DVD-stop timers.
 */
void VIInit(void) {
    u16 dspCfg;
    u32 value;
    u32 tv;
    u32 tvInBootrom;

    if (IsInitialized) {
        return;
    }

    OSRegisterVersion(__VIVersion);
    IsInitialized = TRUE;

    if (!(__VIRegs[1] & 1)) {
        __VIInit(VI_TVMODE_NTSC_INT);
    }

    retraceCount = 0;
    changed = 0;
    shdwChanged = 0;
    changeMode = 0;
    shdwChangeMode = 0;
    flushFlag = 0;
    flushFlag3in1 = 0;

    __VIRegs[39] = taps[0] | ((taps[1] & 0x3F) << 10);
    __VIRegs[38] = (taps[1] >> 6) | (taps[2] << 4);
    __VIRegs[41] = taps[3] | ((taps[4] & 0x3F) << 10);
    __VIRegs[40] = (taps[4] >> 6) | (taps[5] << 4);
    __VIRegs[43] = taps[6] | ((taps[7] & 0x3F) << 10);
    __VIRegs[42] = (taps[7] >> 6) | (taps[8] << 4);
    __VIRegs[45] = taps[9] | (taps[10] << 8);
    __VIRegs[44] = taps[11] | (taps[12] << 8);
    __VIRegs[47] = taps[13] | (taps[14] << 8);
    __VIRegs[46] = taps[15] | (taps[16] << 8);
    __VIRegs[49] = taps[17] | (taps[18] << 8);
    __VIRegs[48] = taps[19] | (taps[20] << 8);
    __VIRegs[51] = taps[21] | (taps[22] << 8);
    __VIRegs[50] = taps[23] | (taps[24] << 8);
    __VIRegs[56] = 0x280;
    ImportAdjustingValues();

    tvInBootrom = *(u32*)OSPhysicalToCached(0xCC);
    dspCfg = __VIRegs[1];
    HorVer.nonInter = VIGetScanMode();
    HorVer.tv = ((u32)(dspCfg) & 0x300) >> 8;

    if (tvInBootrom == VI_EURGB60 || (tvInBootrom == VI_PAL && HorVer.tv == VI_NTSC)) {
        HorVer.tv = VI_EURGB60;
    }

    tv = (HorVer.tv == 3) ? 0 : HorVer.tv;
    HorVer.timing = getTiming((tv << 2) + HorVer.nonInter);
    regs[1] = dspCfg;

    CurrTiming = HorVer.timing;
    CurrTvMode = HorVer.tv;

    HorVer.DispSizeX = 640;
    HorVer.DispSizeY = CurrTiming->acv * 2;
    HorVer.DispPosX = (720 - HorVer.DispSizeX) / 2;
    HorVer.DispPosY = 0;
    AdjustPosition(CurrTiming->acv);
    HorVer.FBSizeX = 640;
    HorVer.FBSizeY = CurrTiming->acv * 2;
    HorVer.PanPosX = 0;
    HorVer.PanPosY = 0;
    HorVer.PanSizeX = 640;
    HorVer.PanSizeY = CurrTiming->acv * 2;
    HorVer.FBMode = 0;

    HorVer.wordPerLine = 40;
    HorVer.std = 40;
    HorVer.wpl = 40;
    HorVer.xof = 0;
    HorVer.black = 1;
    HorVer.threeD = 0;
    OSInitThreadQueue(&retraceQueue);
    value = __VIRegs[24];
    value &= ~VI_DI_INTERRUPT;
    value = (u16)value;
    __VIRegs[24] = value;
    value = __VIRegs[26];
    value = value & ~VI_DI_INTERRUPT;
    value = (u16)value;
    __VIRegs[26] = value;
    PreCB = NULL;
    PostCB = NULL;
    __OSSetInterruptHandler(0x18, __VIRetraceHandler);
    __OSUnmaskInterrupts(0x80);
    OSRegisterShutdownFunction(&ShutdownFunctionInfo);

    switch (VIGetTvFormat()) {
    case VI_PAL:
        THD_TIME_TO_DIMMING = VI_DIM_5M_50HZ;
        NEW_TIME_TO_DIMMING = VI_DIM_5M_50HZ;
        THD_TIME_TO_DVD_STOP = VI_DVD_STOP_50HZ;
        break;
    default:
        THD_TIME_TO_DIMMING = VI_DIM_5M_60HZ;
        NEW_TIME_TO_DIMMING = VI_DIM_5M_60HZ;
        THD_TIME_TO_DVD_STOP = VI_DVD_STOP_60HZ;
        break;
    }

    _gIdleCount_dimming = 0;
    _gIdleCount_dvd = 0;
    g_current_time_to_dim = VI_DM_DEFAULT;
    __VIDimming_All_Clear = TRUE;
    __VIDimmingState = FALSE;
    VIEnableDimming(TRUE);

    VIEnableDVDStopMotor(FALSE);
    __VISetRevolutionModeSimple();
} // 0x80228CC0

/*
 * VIWaitForRetrace
 *
 * Blocks the calling thread until the next retrace.
 */
void VIWaitForRetrace(void) {
    BOOL enabled;
    u32 count;

    enabled = OSDisableInterrupts();
    count = retraceCount;
    do {
        OSSleepThread(&retraceQueue);
    } while (count == retraceCount);
    OSRestoreInterrupts(enabled);
} // 0x80229210

/*
 * setInterruptRegs
 *
 * Programs the retrace interrupt position from the timing row.
 */
static void setInterruptRegs(timing_s* tm) {
    u16 hct, vct;
    u16 borrow;

    vct = tm->nhlines / 2;
    borrow = tm->nhlines % 2;
    hct = (u16)((borrow)? tm->hlw : (u16)0);
    vct++;
    hct++;
    regs[25] = (u16)(u32)hct;
    MARK_CHANGED(25);
    regs[24] = vct | VI_DI_ENABLE;
    MARK_CHANGED(24);
}

/*
 * setPicConfig
 *
 * Derives the frame buffer line geometry (words per line, stride, x offset) and the picture config register.
 */
static void setPicConfig(u16 fbSizeX, VIXFBMode xfbMode, u16 panPosX, u16 panSizeX, u8* wordPerLine, u8* std, u8* wpl, u8* xof) {
    *wordPerLine = (fbSizeX + 15) / 16;
    *std = (xfbMode == VI_XFBMODE_SF) ? *wordPerLine : (u8)(*wordPerLine * 2);
    *xof = panPosX % 16;
    *wpl = (*xof + panSizeX + 15) / 16;
    regs[0x24] = *std | (*wpl << 8);
    MARK_CHANGED(36);
}

/*
 * setBBIntervalRegs
 *
 * Programs the four burst blanking interval registers.
 */
static void setBBIntervalRegs(timing_s* tm) {
    u16 val;

    val = tm->bs1 | (tm->be1 << 5);
    regs[11] = val;
    MARK_CHANGED(11);

    val = tm->bs3 | (tm->be3 << 5);
    regs[10] = val;
    MARK_CHANGED(10);

    val = tm->bs2 | (tm->be2 << 5);
    regs[13] = val;
    MARK_CHANGED(13);

    val = tm->bs4 | (tm->be4 << 5);
    regs[12] = val;
    MARK_CHANGED(12);
}

/*
 * setScalingRegs
 *
 * Programs the horizontal scaler from the pan and display widths.
 */
static void setScalingRegs(u16 panSizeX, u16 dispSizeX, BOOL threeD) {
    u32 scale;

    panSizeX = threeD ? (panSizeX << 1) : panSizeX;
    if (panSizeX < dispSizeX) {
        scale = (u32)(dispSizeX + (panSizeX << 8) - 1) / dispSizeX;
        regs[37] = scale | 0x1000;
        MARK_CHANGED(37);
        regs[56] = (u32)panSizeX;
        MARK_CHANGED(56);
    } else {
        regs[37] = 0x100;
        MARK_CHANGED(37);
    }
}

/*
 * calcFbbs
 *
 * Top and bottom field buffer addresses of a frame buffer (swapped for an odd display origin).
 */
static void calcFbbs(u32 bufAddr, u16 panPosX, u16 panPosY, u8 wordPerLine, VIXFBMode xfbMode, u16 dispPosY, u32* tfbb, u32* bfbb) {
    u32 bytesPerLine;
    u32 xoffInWords;

    xoffInWords = (u32)panPosX / 16;
    bytesPerLine = (u32)wordPerLine * 32;
    *tfbb = bufAddr + (xoffInWords << 5) + (bytesPerLine * panPosY);
    *bfbb = (xfbMode == VI_XFBMODE_SF) ? *tfbb : *tfbb + bytesPerLine;

    if (dispPosY % 2 == 1) {
        u32 tmp = *tfbb;
        *tfbb = *bfbb;
        *bfbb = tmp;
    }

    *tfbb &= VI_FBB_PHYS_MASK;
    *bfbb &= VI_FBB_PHYS_MASK;
}

/*
 * setFbbRegs
 *
 * Programs the field buffer base registers (left and, for 3D, right eye); addresses above 16MB are
 * stored shifted right by 5.
 */
static void setFbbRegs(VIDisplayState* HorVer, u32* tfbb, u32* bfbb, u32* rtfbb, u32* rbfbb) {
    u32 shifted;

    calcFbbs(HorVer->bufAddr, HorVer->PanPosX, HorVer->AdjustedPanPosY, HorVer->wordPerLine, HorVer->FBMode, HorVer->AdjustedDispPosY, tfbb, bfbb);
    if (HorVer->threeD) {
        calcFbbs(HorVer->rbufAddr, HorVer->PanPosX, HorVer->AdjustedPanPosY, HorVer->wordPerLine, HorVer->FBMode, HorVer->AdjustedDispPosY, rtfbb, rbfbb);
    }

    if (*tfbb < VI_FBB_SHIFT_MIN && *bfbb < VI_FBB_SHIFT_MIN && *rtfbb < VI_FBB_SHIFT_MIN && *rbfbb < VI_FBB_SHIFT_MIN) {
        shifted = 0;
    } else {
        shifted = 1;
    }

    if (shifted) {
        *tfbb >>= 5;
        *bfbb >>= 5;
        *rtfbb >>= 5;
        *rbfbb >>= 5;
    }

    regs[15] = (u16)(*tfbb & 0xFFFF);
    MARK_CHANGED(15);
    regs[14] = (shifted << 12) | ((*tfbb >> 16) | (HorVer->xof << 8));
    MARK_CHANGED(14);
    regs[19] = (u16)(*bfbb & 0xFFFF);
    MARK_CHANGED(19);
    regs[18] = (*bfbb >> 16);
    MARK_CHANGED(18);

    if (HorVer->threeD) {
        regs[17] = (u16)(*rtfbb & 0xFFFF);
        MARK_CHANGED(17);
        regs[16] = *rtfbb >> 16;
        MARK_CHANGED(16);
        regs[21] = (u16)(*rbfbb & 0xFFFF);
        MARK_CHANGED(21);
        regs[20] = *rbfbb >> 16;
        MARK_CHANGED(20);
    }
} // 0x80229270

/*
 * setHorizontalRegs
 *
 * Programs the horizontal timing and blanking registers for the display origin and width.
 */
static void setHorizontalRegs(timing_s* tm, u16 dispPosX, u16 dispSizeX) {
    u32 hbe;
    u32 hbs;
    u32 hbeLo;
    u32 hbeHi;

    regs[3] = (u16)(u32)tm->hlw;
    MARK_CHANGED(3);
    regs[2] = tm->hce | (tm->hcs << 8);
    MARK_CHANGED(2);

    if (HorVer.tv == 8) {
        hbe = (u32)(tm->hbe640 + 172);
        hbs = tm->hbs640;
    } else {
        hbe = (u32)(tm->hbe640 - 40 + dispPosX);
        hbs = (u32)(tm->hbs640 + 40 + dispPosX - (720 - dispSizeX));
    }

    hbeLo = hbe & 0x1FF;
    hbeHi = hbe >> 9;

    regs[5] = tm->hsy | (hbeLo << 7);
    MARK_CHANGED(5);
    regs[4] = hbeHi | (hbs * 2);
    MARK_CHANGED(4);
} // 0x80229520

/*
 * setVerticalRegs
 *
 * Programs the vertical timing registers; black output sets zero active lines.
 */
static void setVerticalRegs(u16 dispPosY, u16 dispSizeY, u8 equ, u16 acv, u16 prbOdd, u16 prbEven, u16 psbOdd, u16 psbEven, BOOL black) {
    u16 actualPrbOdd;
    u16 actualPrbEven;
    u16 actualPsbOdd;
    u16 actualPsbEven;
    u16 actualAcv;
    u16 c;
    u16 d;

    if (HorVer.nonInter == 2 || HorVer.nonInter == 3) {
        c = 1;
        d = 2;
    } else {
        c = 2;
        d = 1;
    }

    if ((dispPosY % 2) == 0) {
        actualPrbOdd = prbOdd + (d * dispPosY);
        actualPsbOdd = psbOdd + (d * (((c * acv) - dispSizeY) - dispPosY));
        actualPrbEven = prbEven + (d * dispPosY);
        actualPsbEven = psbEven + (d * (((c * acv) - dispSizeY) - dispPosY));
    } else {
        actualPrbOdd = prbEven + (d * dispPosY);
        actualPsbOdd = psbEven + (d * (((c * acv) - dispSizeY) - dispPosY));
        actualPrbEven = prbOdd + (d * dispPosY);
        actualPsbEven = psbOdd + (d * (((c * acv) - dispSizeY) - dispPosY));
    }

    actualAcv = dispSizeY / c;

    if (black) {
        actualPrbOdd += 2 * actualAcv - 2;
        actualPsbOdd += 2;
        actualPrbEven += 2 * actualAcv - 2;
        actualPsbEven += 2;
        actualAcv = 0;
    }

    regs[0] = equ | (actualAcv << 4);
    MARK_CHANGED(0);
    regs[7] = (u16)(u32)actualPrbOdd;
    MARK_CHANGED(7);
    regs[6] = (u16)(u32)actualPsbOdd;
    MARK_CHANGED(6);
    regs[9] = (u16)(u32)actualPrbEven;
    MARK_CHANGED(9);
    regs[8] = (u16)(u32)actualPsbEven;
    MARK_CHANGED(8);
} // 0x80229600

/*
 * PrintDebugPalCaution
 *
 * Prints a one-time warning when the DEBUG_PAL TV format is configured.
 */
static void PrintDebugPalCaution(void) {
    static u32 message;

    if (message == 0) {
        message = 1;
        OSReport("***************************************\n");
        OSReport(" ! ! ! C A U T I O N ! ! !             \n");
        OSReport("This TV format \"DEBUG_PAL\" is only for \n");
        OSReport("temporary solution until PAL DAC board \n");
        OSReport("is available. Please do NOT use this   \n");
        OSReport("mode in real games!!!                  \n");
        OSReport("***************************************\n");
    }
}

/*
 * VIConfigure
 *
 * Configures the video interface for a render mode (TV format, scan mode, picture geometry).
 */
void VIConfigure(const GXRenderModeObj* rm) {
    timing_s* tm;
    u32 regDspCfg;
    u32 regClksel;
    BOOL enabled;
    u32 newNonInter;
    u32 tvInBootrom;
    u32 tvInGame;

    enabled = OSDisableInterrupts();
    newNonInter = rm->viTVmode & 3;

    if (HorVer.nonInter != newNonInter) {
        changeMode = 1;
        HorVer.nonInter = newNonInter;
    }

    tvInGame = (u32)rm->viTVmode >> 2;
    tvInBootrom = *(u32*)OSPhysicalToCached(0xCC);

    if (tvInGame == VI_DEBUG_PAL) {
        PrintDebugPalCaution();
    }

    if (((tvInBootrom != VI_PAL && tvInBootrom != VI_EURGB60) && (tvInGame == VI_PAL || tvInGame == VI_EURGB60)) ||
        ((tvInBootrom == VI_PAL || tvInBootrom == VI_EURGB60) && (tvInGame != VI_PAL && tvInGame != VI_EURGB60)))
    {
        OSPanic(__FILE__, 2651, "VIConfigure(): Tried to change mode from (%d) to (%d), which is forbidden\n", tvInBootrom, tvInGame);
    }

    if ((tvInGame == VI_NTSC) || (tvInGame == VI_MPAL)) {
        HorVer.tv = tvInBootrom;
    } else {
        HorVer.tv = tvInGame;
    }

    HorVer.DispPosX  = rm->viXOrigin;
    HorVer.DispPosY  = (HorVer.nonInter == 1) ? (u16)(rm->viYOrigin * 2) : rm->viYOrigin;
    HorVer.DispSizeX = rm->viWidth;
    HorVer.FBSizeX   = rm->fbWidth;
    HorVer.FBSizeY   = rm->xfbHeight;
    HorVer.FBMode    = rm->xFBmode;
    HorVer.PanSizeX  = HorVer.FBSizeX;
    HorVer.PanSizeY  = HorVer.FBSizeY;
    HorVer.PanPosX   = 0;
    HorVer.PanPosY   = 0;
    HorVer.DispSizeY = (HorVer.nonInter == 2)           ? HorVer.PanSizeY :
                       (HorVer.nonInter == 3)           ? HorVer.PanSizeY :
                       (HorVer.FBMode == VI_XFBMODE_SF) ? (u16)(HorVer.PanSizeY * 2) :
                                                          HorVer.PanSizeY;
    HorVer.threeD = (HorVer.nonInter == 3) ? TRUE : FALSE;

    tm = getTiming((HorVer.tv << 2) + HorVer.nonInter);
    HorVer.timing = tm;

    AdjustPosition(tm->acv);

    setInterruptRegs(tm);

    regDspCfg = regs[1];
    regClksel = regs[54];

    if (HorVer.nonInter == VI_PROGRESSIVE || HorVer.nonInter == 3) {
        regDspCfg = (((u32)(regDspCfg)) & ~0x00000004) | (((u32)(1)) << 2);

        if (HorVer.tv == VI_HD720) {
            regClksel = (((u32)(regClksel)) & ~0x00000001) | 0;
        } else {
            regClksel = (((u32)(regClksel)) & ~0x00000001) | 1;
        }

    } else {
        OLD_SET_REG_FIELD(2722, regDspCfg, 1, 2, HorVer.nonInter & 1);
        regClksel = (((u32)(regClksel)) & ~0x00000001);
    }

    OLD_SET_REG_FIELD(2726, regDspCfg, 1, 3, HorVer.threeD);

    if ((HorVer.tv == VI_PAL) || (HorVer.tv == VI_MPAL) || (HorVer.tv == 3)) {
        OLD_SET_REG_FIELD(2730, regDspCfg, 2, 8, HorVer.tv);
    } else {
        regDspCfg = (((u32)(regDspCfg)) & ~0x00000300);
    }

    regs[1] = regDspCfg;
    regs[54] = (u16)regClksel;

    MARK_CHANGED(1);
    MARK_CHANGED(54);

    setScalingRegs(HorVer.PanSizeX, HorVer.DispSizeX, HorVer.threeD);
    setHorizontalRegs(tm, HorVer.AdjustedDispPosX, HorVer.DispSizeX);
    setBBIntervalRegs(tm);
    setPicConfig(HorVer.FBSizeX, HorVer.FBMode, HorVer.PanPosX, HorVer.PanSizeX, &HorVer.wordPerLine, &HorVer.std, &HorVer.wpl, &HorVer.xof);
    if (FBSet != 0) {
        setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    }
    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.AdjustedDispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven, HorVer.black);
    OSRestoreInterrupts(enabled);
} // 0x802297A0

/*
 * VIConfigurePan
 *
 * Sets the visible window of the frame buffer.
 */
void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
    BOOL enabled;
    timing_s* tm;

    enabled = OSDisableInterrupts();
    HorVer.PanPosX   = xOrg;
    HorVer.PanPosY   = yOrg;
    HorVer.PanSizeX  = width;
    HorVer.PanSizeY  = height;
    HorVer.DispSizeY = (HorVer.nonInter == 2)           ? HorVer.PanSizeY :
                       (HorVer.nonInter == 3)           ? HorVer.PanSizeY :
                       (HorVer.FBMode == VI_XFBMODE_SF) ? (u16)(HorVer.PanSizeY * 2) :
                                                          HorVer.PanSizeY;
    tm = HorVer.timing;
    AdjustPosition(tm->acv);
    setScalingRegs(HorVer.PanSizeX, HorVer.DispSizeX, HorVer.threeD);
    setPicConfig(HorVer.FBSizeX, HorVer.FBMode, HorVer.PanPosX, HorVer.PanSizeX, &HorVer.wordPerLine, &HorVer.std, &HorVer.wpl, &HorVer.xof);
    if (FBSet != 0) {
        setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    }
    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.DispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven, HorVer.black);
    OSRestoreInterrupts(enabled);
} // 0x80229E70

/*
 * VIFlush
 *
 * Commits the pending register changes; they take effect at the next retrace.
 */
void VIFlush(void) {
    BOOL enabled;
    s32 regIndex;

    enabled = OSDisableInterrupts();
    shdwChangeMode |= changeMode;
    changeMode = 0;
    shdwChanged |= changed;

    while (changed != 0) {
        regIndex = cntlzd(changed);
        shdwRegs[regIndex] = regs[regIndex];
        changed &= ~((u64)1 << (63 - regIndex));
    }

    flushFlag = 1;
    flushFlag3in1 = 1;
    NextBufAddr = HorVer.bufAddr;
    OSRestoreInterrupts(enabled);
} // 0x8022A1D0

/*
 * VISetNextFrameBuffer
 *
 * Sets the frame buffer shown from the next flush.
 */
void VISetNextFrameBuffer(void* fb) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    HorVer.bufAddr = (u32)fb;
    FBSet = 1;
    setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    OSRestoreInterrupts(enabled);
} // 0x8022A2F0

/*
 * VISetBlack
 *
 * Blanks or restores the video output.
 */
void VISetBlack(BOOL black) {
    BOOL enabled;
    timing_s* tm;

    enabled = OSDisableInterrupts();
    HorVer.black = black;
    tm = HorVer.timing;
    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.DispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven, HorVer.black);
    OSRestoreInterrupts(enabled);
} // 0x8022A360

/*
 * VIGetRetraceCount
 *
 * Number of retraces since VIInit.
 */
u32 VIGetRetraceCount(void) {
    return retraceCount;
} // 0x8022A3E0

/*
 * GetCurrentDisplayPosition
 *
 * Reads the beam position, repeating until the vertical count is stable.
 */
static void GetCurrentDisplayPosition(u32* hct, u32* vct) {
    u32 hcount, vcount0, vcount;
    vcount = __VIRegs[VI_VERT_COUNT] & 0x7FF;

    do {
        vcount0 = vcount;
        hcount = __VIRegs[VI_HORIZ_COUNT] & 0x7FF;
        vcount = __VIRegs[VI_VERT_COUNT] & 0x7FF;
    } while (vcount0 != vcount);

    *hct = hcount;
    *vct = vcount;
}

/*
 * getCurrentHalfLine
 *
 * Beam position as a half-line count from the start of the frame.
 */
static u32 getCurrentHalfLine(void) {
    u32 hcount, vcount;
    GetCurrentDisplayPosition(&hcount, &vcount);

    return ((vcount - 1) << 1) + ((hcount - 1) / CurrTiming->hlw);
}

/*
 * getCurrentFieldEvenOdd
 *
 * 1 while the beam is in the first field of the frame, 0 in the second.
 */
static u32 getCurrentFieldEvenOdd(void) {
    return (getCurrentHalfLine() < CurrTiming->nhlines) ? 1 : 0;
}

/*
 * VIGetNextField
 *
 * Parity of the field that will be shown next.
 */
u32 VIGetNextField(void) {
    s32 nextField;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    nextField = getCurrentFieldEvenOdd() ^ 1;
    OSRestoreInterrupts(enabled);
    return nextField ^ (HorVer.AdjustedDispPosY & 1);
} // 0x8022A3F0

/*
 * VIGetCurrentLine
 *
 * Scan line the beam is on within the current field.
 */
u32 VIGetCurrentLine(void) {
    u32 halfLine;
    timing_s* tm;
    BOOL enabled;

    tm = CurrTiming;
    enabled = OSDisableInterrupts();
    halfLine = getCurrentHalfLine();
    OSRestoreInterrupts(enabled);
    if (halfLine >= tm->nhlines) {
        halfLine -= tm->nhlines;
    }
    return halfLine >> 1U;
} // 0x8022A490

/*
 * VIGetTvFormat
 *
 * TV format of the current video mode (NTSC, PAL, MPAL or EURGB60); the debug and extra modes report NTSC or PAL.
 * The debug build asserts on an unknown mode; the release build leaves the result unset.
 */
u32 VIGetTvFormat(void) {
    u32 format;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    switch (CurrTvMode) {
    case VI_NTSC:
    case VI_DEBUG:
    case 6:
    case 7:
    case 8:
        format = VI_NTSC;
        break;
    case VI_PAL:
    case VI_DEBUG_PAL:
        format = VI_PAL;
        break;
    case VI_EURGB60:
    case VI_MPAL:
        format = CurrTvMode;
        break;
    }

    OSRestoreInterrupts(enabled);
    return format;
} // 0x8022A520

/*
 * VIGetScanMode
 *
 * Scan mode the hardware is running (interlaced, non-interlaced or progressive).
 */
u32 VIGetScanMode(void) {
    u32 scanMode;
    BOOL enabled = OSDisableInterrupts();

    if ((u32)(__VIRegs[54] & 1) == 1) {
        scanMode = 2;
    } else if (((u32)(__VIRegs[1] & 4) >> 2) == 0) {
        scanMode = 0;
    } else {
        scanMode = 1;
    }

    OSRestoreInterrupts(enabled);
    return scanMode;
} // 0x8022A580

/*
 * VIGetDTVStatus
 *
 * Whether a component (DTV) cable is attached.
 */
u32 VIGetDTVStatus(void) {
    u32 dtvStatus;
    BOOL enabled = OSDisableInterrupts();

    dtvStatus = __VIRegs[55] & 3;
    OSRestoreInterrupts(enabled);
    return dtvStatus & 1;
} // 0x8022A5E0

/*
 * __VIDisplayPositionToXY
 *
 * Converts a beam position to picture coordinates (y is -1 outside the active area).
 */
void __VIDisplayPositionToXY(u32 hct, u32 vct, s16* x, s16* y) {
    u32 halfLine = ((vct - 1) << 1) + ((hct - 1) / CurrTiming->hlw);

    if (HorVer.nonInter == VI_INTERLACE) {
        if (halfLine < CurrTiming->nhlines) {
            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbOdd) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbOdd) {
                *y = -1;
            } else {
                *y = (s16)((halfLine - CurrTiming->equ * 3 - CurrTiming->prbOdd) & ~1);
            }
        } else {
            halfLine -= CurrTiming->nhlines;

            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbEven) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbEven) {
                *y = -1;
            } else {
                *y = (s16)(((halfLine - CurrTiming->equ * 3 - CurrTiming->prbEven) & ~1) + 1);
            }
        }
    } else if (HorVer.nonInter == VI_NON_INTERLACE) {
        if (halfLine >= CurrTiming->nhlines) {
            halfLine -= CurrTiming->nhlines;
        }

        if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbOdd) {
            *y = -1;
        } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbOdd) {
            *y = -1;
        } else {
            *y = (s16)((halfLine - CurrTiming->equ * 3 - CurrTiming->prbOdd) & ~1);
        }
    } else if (HorVer.nonInter == VI_PROGRESSIVE) {
        if (halfLine < CurrTiming->nhlines) {
            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbOdd) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbOdd) {
                *y = -1;
            } else {
                *y = (s16)(halfLine - CurrTiming->equ * 3 - CurrTiming->prbOdd);
            }
        } else {
            halfLine -= CurrTiming->nhlines;

            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbEven) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbEven) {
                *y = -1;
            } else {
                *y = (s16)((halfLine - CurrTiming->equ * 3 - CurrTiming->prbEven) & ~1);
            }
        }
    }

    *x = (s16)(hct - 1);
} // 0x8022A620

/*
 * __VIGetCurrentPosition
 *
 * Picture coordinates of the beam.
 */
void __VIGetCurrentPosition(s16* x, s16* y) {
    u32 hcount, vcount;
    GetCurrentDisplayPosition(&hcount, &vcount);
    __VIDisplayPositionToXY(hcount, vcount, x, y);
}

/*
 * VIEnableDimming
 *
 * Enables the screen saver (honouring the system setting); returns the previous state.
 */
BOOL VIEnableDimming(BOOL enable) {
    u8 value;
    BOOL old = __VIDimmingFlag_Enable;

    if (enable == TRUE) {
        value = SCGetScreenSaverMode();

        if (value == 0) {
            enable = FALSE;
        }
    }

    __VIDimmingFlag_Enable = enable;
    return old;
}

/*
 * VISetTimeToDimming
 *
 * Selects the idle time before dimming and recomputes the frame count for the current TV format; returns the
 * previous choice. Name invented.
 */
VITimeToDIM VISetTimeToDimming(VITimeToDIM time) {
    VITimeToDIM old = g_current_time_to_dim;

    g_current_time_to_dim = time;
    switch (VIGetTvFormat()) {
    case VI_PAL:
        switch (g_current_time_to_dim) {
        case VI_DM_10M:
            NEW_TIME_TO_DIMMING = VI_DIM_10M_50HZ;
            break;
        case VI_DM_15M:
            NEW_TIME_TO_DIMMING = VI_DIM_15M_50HZ;
            break;
        default:
            NEW_TIME_TO_DIMMING = VI_DIM_5M_50HZ;
            break;
        }
        break;
    default:
        switch (g_current_time_to_dim) {
        case VI_DM_10M:
            NEW_TIME_TO_DIMMING = VI_DIM_10M_60HZ;
            break;
        case VI_DM_15M:
            NEW_TIME_TO_DIMMING = VI_DIM_15M_60HZ;
            break;
        default:
            NEW_TIME_TO_DIMMING = VI_DIM_5M_60HZ;
            break;
        }
        break;
    }
    return old;
} // 0x8022A860

/*
 * VIResetDimmingCount
 *
 * Restarts the idle count on behalf of the controllers (device 0).
 */
BOOL VIResetDimmingCount(void) {
    return __VIResetDev0Idle();
} // 0x8022A960

/*
 * VIResetRFIdle
 *
 * Restarts the idle count on behalf of the RF device.
 */
BOOL VIResetRFIdle(void) {
    return __VIResetRFIdle();
}

/*
 * VIEnableDVDStopMotor
 *
 * Enables stopping the DVD motor when idle; returns the previous state.
 */
BOOL VIEnableDVDStopMotor(BOOL enable) {
    BOOL old = __VIDVDStopFlag_Enable;
    __VIDVDStopFlag_Enable = enable;
    return old;
}

/*
 * __VIResetSIIdle
 *
 * Restarts the idle count on behalf of the serial interface.
 */
BOOL __VIResetSIIdle(void) {
    __VIDimmingFlag_SI_IDLE = 0;
    return TRUE;
}

/*
 * __VIResetRFIdle
 *
 * Restarts the idle count on behalf of the RF device.
 */
BOOL __VIResetRFIdle(void) {
    __VIDimmingFlag_RF_IDLE = 0;
    return TRUE;
} // 0x8022A980

/*
 * __VIResetDev0Idle
 *
 * Restarts the idle count on behalf of device 0.
 */
BOOL __VIResetDev0Idle(void) {
    __VIDimmingFlag_DEV_IDLE[0] = 0;
    return TRUE;
}
