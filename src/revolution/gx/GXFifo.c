// Reference: zeldaret/tp libs/revolution/src/gx/GXFifo.c
// Reference: doldecomp/ogws GXFifo.h (GXGetCPUFifo/GXGetGPFifo signatures)
// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/gx/GXFifo.c (R4QK01: const GXSetCPUFifo/GXSetGPFifo parameters)
#include <revolution/base/PPCArch.h>
#include <revolution/gx.h>
#include <revolution/os.h>

#include <revolution/gx/__gx.h>

#define TOPHYSICAL(a) (((u32)(a)) & 0x3FFFFFFF)

static __GXFifoObj CPUFifo;
static __GXFifoObj GPFifo;
static GXBool CPUFifoReady = FALSE;
static GXBool GPFifoReady = FALSE;

static GXBool CPGPLinked;
static OSThread* __GXCurrentThread;
static BOOL GXOverflowSuspendInProgress;
static GXBreakPtCallback BreakPointCB;
void* __GXCurrentBP;
static u32 __GXOverflowCount;

#if DEBUG
static BOOL IsWGPipeRedirected;
#endif

static void __GXFifoReadEnable(void);
static void __GXFifoReadDisable(void);
static void __GXFifoLink(u8 en);
static void __GXWriteFifoIntEnable(u8 hiWatermarkEn, u8 loWatermarkEn);
static void __GXWriteFifoIntReset(u8 hiWatermarkClr, u8 loWatermarkClr);
GXBool CPGPLinkCheck(void);

/*
 * GXOverflowHandler
 *
 * FIFO high-watermark interrupt: suspends the thread that feeds the FIFO.
 * Inlined into its callers in retail.
 */
static void GXOverflowHandler(__OSInterrupt interrupt, OSContext* context) {
#if DEBUG
    if (__gxVerif->verifyLevel > GX_WARN_SEVERE) {
        OSReport("[GXOverflowHandler]");
    }
#endif
    ASSERT(!GXOverflowSuspendInProgress);

    __GXOverflowCount++;
    __GXWriteFifoIntEnable(0, 1);
    __GXWriteFifoIntReset(1, 0);
    GXOverflowSuspendInProgress = TRUE;

#if DEBUG
    if (__gxVerif->verifyLevel > GX_WARN_SEVERE) {
        OSReport("[GXOverflowHandler Sleeping]");
    }
#endif
    OSSuspendThread(__GXCurrentThread);
}

/*
 * GXUnderflowHandler
 *
 * FIFO low-watermark interrupt: resumes the suspended thread. Inlined into
 * its callers in retail.
 */
static void GXUnderflowHandler(s16 interrupt, OSContext* context) {
#if DEBUG
    if (__gxVerif->verifyLevel > GX_WARN_SEVERE) {
        OSReport("[GXUnderflowHandler]");
    }
#endif
    ASSERT(GXOverflowSuspendInProgress);

    OSResumeThread(__GXCurrentThread);
    GXOverflowSuspendInProgress = FALSE;
    __GXWriteFifoIntReset(1, 1);
    __GXWriteFifoIntEnable(1, 0);
}

/*
 * GXBreakPointHandler
 *
 * FIFO breakpoint interrupt: disables the breakpoint and calls the user
 * callback. Inlined into its callers in retail.
 */
static void GXBreakPointHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;

    SC_CP_REG_ENABLE_SET_FIFOBRKINT(0, __GXData->cpEnable, 0);
    GX_CP_REG_WRITE_U16(CP_ENABLE, __GXData->cpEnable);
    if (BreakPointCB != 0) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        BreakPointCB();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
    }
}

/*
 * GXCPInterruptHandler
 *
 * Command processor interrupt handler; dispatches the underflow, overflow
 * and breakpoint conditions.
 */
// 0x80238930
static void GXCPInterruptHandler(__OSInterrupt interrupt, OSContext* context) {
    __GXData->cpStatus = GX_GET_CP_REG(0);
    if (CP_REG_ENABLE_GET_UNFLINT(__GXData->cpEnable) && CP_REG_STATUS_GET_UNFL(__GXData->cpStatus)) {
        GXUnderflowHandler(interrupt, context);
    }

    if (CP_REG_ENABLE_GET_OVFLINT(__GXData->cpEnable) && CP_REG_STATUS_GET_OVFL(__GXData->cpStatus)) {
        GXOverflowHandler(interrupt, context);
    }

    if (CP_REG_ENABLE_GET_FIFOBRKINT(__GXData->cpEnable) && CP_REG_STATUS_GET_FIFOBRK(__GXData->cpStatus)) {
        GXBreakPointHandler(interrupt, context);
    }
}

/*
 * GXInitFifoBase
 *
 * Initialises a FIFO object over a buffer with default watermarks and
 * empty read/write pointers.
 */
// 0x80238A90
void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    ASSERTMSG(((u32)base & 0x1F) == 0, "GXInitFifoBase: base must be 32B aligned");
    ASSERTMSG(base != 0,               "GXInitFifoBase: base pointer is NULL");
    ASSERTMSG((size & 0x1F) == 0,      "GXInitFifoBase: size must be 32B aligned");
    ASSERTMSG(size >= 0x10000,         "GXInitFifoBase: fifo is not large enough");  // minimum FIFO size is 64 KiB

    realFifo->base = base;
    realFifo->top = (u8*)base + size - 4;
    realFifo->size = size;
    realFifo->count = 0;
    GXInitFifoLimits(fifo, size - 0x4000, (size >> 1) & ~0x1F);  // hi watermark: 16 KiB below full; lo: half
    GXInitFifoPtrs(fifo, base, base);
}

/*
 * GXInitFifoPtrs
 *
 * Sets the read and write pointers of a FIFO object. Dead-stripped in
 * retail.
 */
void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr) {
    __GXFifoObj* realFifo = (__GXFifoObj *)fifo;
    BOOL enabled;

    ASSERTMSG(((u32)readPtr & 0x1F) == 0,  "GXInitFifoPtrs: readPtr not 32B aligned");
    ASSERTMSG(((u32)writePtr & 0x1F) == 0, "GXInitFifoPtrs: writePtr not 32B aligned");
    ASSERTMSG(realFifo->base <= readPtr && readPtr < realFifo->top,   "GXInitFifoPtrs: readPtr not in fifo range");
    ASSERTMSG(realFifo->base <= writePtr && writePtr < realFifo->top, "GXInitFifoPtrs: writePtr not in fifo range");

    enabled = OSDisableInterrupts();
    realFifo->rdPtr = readPtr;
    realFifo->wrPtr = writePtr;
    realFifo->count = (u8*)writePtr - (u8*)readPtr;
    if (realFifo->count < 0) {
        realFifo->count += realFifo->size;
    }
    OSRestoreInterrupts(enabled);
}

/*
 * GXInitFifoLimits
 *
 * Sets the high and low watermarks of a FIFO object. Dead-stripped in
 * retail.
 */
void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark, u32 loWatermark) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    ASSERTMSG((hiWatermark & 0x1F) == 0, "GXInitFifoLimits: hiWatermark not 32B aligned");
    ASSERTMSG((loWatermark & 0x1F) == 0, "GXInitFifoLimits: loWatermark not 32B aligned");
    ASSERTMSG(hiWatermark < realFifo->top - realFifo->base, "GXInitFifoLimits: hiWatermark too large");
    ASSERTMSG(loWatermark < hiWatermark, "GXInitFifoLimits: hiWatermark below lo watermark");

    realFifo->hiWatermark = hiWatermark;
    realFifo->loWatermark = loWatermark;
}

/*
 * CPGPLinkCheck
 *
 * Tells whether the CPU and GP FIFOs are the same buffer (linked), and
 * checks that they do not partially overlap.
 */
// 0x80238B20
GXBool CPGPLinkCheck(void) {
    u32 check = 0;
    s32 range1, range2, overlap;

    if (!CPUFifoReady || !GPFifoReady) {
        return GX_FALSE;
    }

    if (CPUFifo.base == GPFifo.base) {
        ++check;
    }

    if (CPUFifo.top == GPFifo.top) {
        ++check;
    }

    if (check == 2) {
        if (CPUFifo.hiWatermark == GPFifo.hiWatermark) {
            ++check;
        }

        if (CPUFifo.loWatermark == GPFifo.loWatermark) {
            ++check;
        }

        ASSERTMSG(check == 4, "GXSetGPFifo/GXSetCPUFifo: mismatch of watermark parameter");
        return GX_TRUE;
    }
    
    range1 = (s32)((u8*)CPUFifo.top - (u8*)GPFifo.base);
    range2 = (s32)((u8*)GPFifo.top - (u8*)CPUFifo.base);
    overlap = ((range1 > 0) && (range2 > 0) || (range1 < 0 && (range2 < 0)));

    if (overlap) {
        OSReport("CPUFifo: %08X - %08X\n", (u32)CPUFifo.base, (u32)CPUFifo.top);
        OSReport("GP Fifo: %08X - %08X\n", (u32)GPFifo.base, (u32)GPFifo.top);
    }

    ASSERTMSG(!overlap, "GXSetGPFifo/GXSetCPUFifo: invalid overlap of both buffers");
    return GX_FALSE;
}

/*
 * GXSetCPUFifo
 *
 * Makes a FIFO the destination of the write-gather pipe, linking it to the
 * GP when both name the same buffer.
 *
 * The parameter is const, as in the 2009 SDK (mscharged R4QK01, Petari): the
 * compiler then assumes the source does not alias CPUFifo and hoists every
 * load of the struct copy above its stores.
 */
// 0x80238C20
void GXSetCPUFifo(const GXFifoObj* fifo) {
    const __GXFifoObj* realFifo = (const __GXFifoObj*)fifo;
    BOOL enabled = OSDisableInterrupts();
    u32 reg;

    if (fifo == NULL) {
        CPUFifoReady = GX_FALSE;
        CPGPLinked = GX_FALSE;
        CPUFifo.bind_cpu = CPUFifo.bind_gp = GX_FALSE;
        OSRestoreInterrupts(enabled);
        return;
    }

    CPUFifo = *realFifo;
    CPUFifoReady = GX_TRUE;
    CPUFifo.bind_cpu = GX_TRUE;

    if (CPGPLinkCheck()) {
        CPGPLinked = GX_TRUE;
        CPUFifo.bind_gp = GX_TRUE;

        reg = 0;

        GX_PI_REG_WRITE_U32(PI_REG_CPBAS, TOPHYSICAL(CPUFifo.base));
        GX_PI_REG_WRITE_U32(PI_REG_CPTOP, TOPHYSICAL(CPUFifo.top));

        SC_PI_CPWRT_REG_SET_CPWRT(787, reg, (GX_PHY_ADDR(TOPHYSICAL(CPUFifo.wrPtr)) >> 5));
        SC_PI_CPWRT_REG_SET_CPWRAP(0, reg, 0);
        GX_PI_REG_WRITE_U32(PI_REG_CPWRT, reg);

        __GXWriteFifoIntReset(1, 1);
        __GXWriteFifoIntEnable(1, 0);
        __GXFifoLink(1);
    } else {
        CPUFifo.bind_gp = GX_FALSE;

        if (CPGPLinked) {
            __GXFifoLink(0);
            CPGPLinked = GX_FALSE;
        }

        __GXWriteFifoIntEnable(0, 0);
        reg = 0;
        GX_PI_REG_WRITE_U32(PI_REG_CPBAS, TOPHYSICAL(CPUFifo.base));
        GX_PI_REG_WRITE_U32(PI_REG_CPTOP, TOPHYSICAL(CPUFifo.top));  
        SC_PI_CPWRT_REG_SET_CPWRT(819, reg, (GX_PHY_ADDR(TOPHYSICAL(CPUFifo.wrPtr)) >> 5));
        SC_PI_CPWRT_REG_SET_CPWRAP(0, reg, 0);
        GX_PI_REG_WRITE_U32(PI_REG_CPWRT, reg);
    }

    PPCSync();
    OSRestoreInterrupts(enabled);
}

/*
 * GXSetGPFifo
 *
 * Makes a FIFO the source of the command processor, linking it to the CPU
 * FIFO when both name the same buffer.
 *
 * The parameter is const (2009 SDK), which lets the struct copy hoist its
 * loads above the stores, as for GXSetCPUFifo.
 */
// 0x80238E00
void GXSetGPFifo(const GXFifoObj* fifo) {
    const __GXFifoObj* realFifo = (const __GXFifoObj*)fifo;
    BOOL enabled;
    u32 stbtmp;

    ASSERTMSG(((u32)realFifo->base & 0x30000000) == 0, "GXSetGPFifo: GX Fifo should be located on MEM1 memory region");  // 0x30000000: bits outside MEM1

    enabled = OSDisableInterrupts();

    __GXFifoReadDisable();
    __GXWriteFifoIntEnable(0, 0);

    if (fifo == NULL) {
        GPFifoReady = GX_FALSE;
        CPGPLinked = GX_FALSE;
        __GXFifoLink(GX_FALSE);
        GPFifo.bind_gp = GPFifo.bind_cpu = GX_FALSE;
        OSRestoreInterrupts(enabled);
        return;
    }

    GPFifo = *realFifo;
    GPFifoReady = GX_TRUE;
    GPFifo.bind_gp = GX_TRUE;
    GX_CP_REG_WRITE_U16(CP_FIFO_BASEL, TOPHYSICAL(GPFifo.base));
    GX_CP_REG_WRITE_U16(CP_FIFO_TOPL, TOPHYSICAL(GPFifo.top));
    GX_CP_REG_WRITE_U16(CP_FIFO_COUNTL, GPFifo.count);
    GX_CP_REG_WRITE_U16(CP_FIFO_WPTRL, TOPHYSICAL(GPFifo.wrPtr));
    GX_CP_REG_WRITE_U16(CP_FIFO_RPTRL, TOPHYSICAL(GPFifo.rdPtr));
    GX_CP_REG_WRITE_U16(CP_FIFO_HICNTL, GPFifo.hiWatermark);
    GX_CP_REG_WRITE_U16(CP_FIFO_LOCNTL, GPFifo.loWatermark);
    GX_CP_REG_WRITE_U16(CP_FIFO_BASEH, TOPHYSICAL(GPFifo.base) >> 16);
    GX_CP_REG_WRITE_U16(CP_FIFO_TOPH, TOPHYSICAL(GPFifo.top) >> 16);
    GX_CP_REG_WRITE_U16(CP_FIFO_COUNTH, GPFifo.count >> 16);
    GX_CP_REG_WRITE_U16(CP_FIFO_WPTRH, TOPHYSICAL(GPFifo.wrPtr) >> 16);
    GX_CP_REG_WRITE_U16(CP_FIFO_RPTRH, TOPHYSICAL(GPFifo.rdPtr) >> 16);
    GX_CP_REG_WRITE_U16(CP_FIFO_HICNTH, GPFifo.hiWatermark >> 16);
    GX_CP_REG_WRITE_U16(CP_FIFO_LOCNTH, GPFifo.loWatermark >> 16);

    PPCSync();

    if (CPGPLinkCheck()) {
        CPGPLinked = GX_TRUE;
        GPFifo.bind_cpu = GX_TRUE;
        __GXWriteFifoIntEnable(GX_TRUE, GX_FALSE);
        __GXFifoLink(GX_TRUE);
    } else {
        CPGPLinked = GX_FALSE;
        GPFifo.bind_cpu = GX_FALSE;
        __GXWriteFifoIntEnable(GX_FALSE, GX_FALSE);
        __GXFifoLink(GX_FALSE);
    }

    stbtmp = __GXData->cpEnable;
    SC_CP_REG_ENABLE_SET_FIFOBRK(0, stbtmp, 0);
    SC_CP_REG_ENABLE_SET_FIFOBRKINT(0, stbtmp, 0);
    GX_CP_REG_WRITE_U16(CP_ENABLE, stbtmp);
    GX_CP_REG_WRITE_U16(CP_ENABLE, __GXData->cpEnable);
    __GXWriteFifoIntReset(GX_TRUE, GX_TRUE);
    __GXFifoReadEnable();
    OSRestoreInterrupts(enabled);
}

#define CP_READ_2U16(dest,reg,type) \
{\
    u32 temp;                                 \
    temp =  (u32)GX_CP_REG_READ_U16(reg##H)<<16;   \
    temp |= (u32)GX_CP_REG_READ_U16(reg##L);       \
    dest = (type)OSPhysicalToCached(temp);   \
}

#define CP_READ_2U16_INT(dest,reg,type) \
{\
    u32 temp;                                 \
    temp =  (u32)GX_CP_REG_READ_U16(reg##H)<<16;   \
    temp |= (u32)GX_CP_REG_READ_U16(reg##L);       \
    dest = (type)temp;   \
}

/*
 * __GXSaveFifoCPStat
 *
 * Reads the CP read pointer and count back into the GP FIFO object.
 * Inlined into its callers in retail.
 */
static void __GXSaveFifoCPStat(void) {
    CP_READ_2U16(GPFifo.rdPtr, CP_FIFO_RPTR, void*);
    CP_READ_2U16_INT(GPFifo.count, CP_FIFO_COUNT, s32);
}

/*
 * __GXSaveFifoPIStat
 *
 * Reads the PI write pointer and wrap flag back into the CPU FIFO object.
 * Inlined into its callers in retail.
 */
static void __GXSaveFifoPIStat(void) {
    u32 reg = GX_PI_REG_READ_U32(PI_REG_CPWRT);
    CPUFifo.wrPtr = OSPhysicalToCached(reg & PI_CPWRT_REG_CPWRT_MASK);
    CPUFifo.wrap = (GXBool)((reg & PI_CPWRT_REG_CPWRAP_MASK) ? GX_TRUE : GX_FALSE);
}

/*
 * __GXSaveFifo
 *
 * Refreshes the CPU and GP FIFO objects from the hardware pointers.
 */
// 0x80239090
void __GXSaveFifo(void) {
    BOOL enabled = OSDisableInterrupts();

    if (CPUFifoReady) {
        __GXSaveFifoPIStat();
    }

    if (GPFifoReady) {
        __GXSaveFifoCPStat();
    }

    if (CPGPLinked) {
        CPUFifo.rdPtr = GPFifo.rdPtr;
        CPUFifo.count = GPFifo.count;
        GPFifo.wrPtr = CPUFifo.wrPtr;
        GPFifo.wrap = CPUFifo.wrap;
    } else if (CPUFifoReady) {
        CPUFifo.count  = (s32)CPUFifo.wrPtr - (s32)CPUFifo.rdPtr;
        if (CPUFifo.count < 0) {
            CPUFifo.count += CPUFifo.size;
        }
    }

    OSRestoreInterrupts(enabled);
}

/*
 * __GXIsGPFifoReady
 *
 * Tells whether a GP FIFO has been set.
 */
// 0x80239190
GXBool __GXIsGPFifoReady(void) {
    return GPFifoReady;
}

/*
 * GXGetGPStatus
 *
 * Reads the command processor status flags.
 */
// 0x802391A0
void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle, GXBool* cmdIdle, GXBool* brkpt) {
    __GXData->cpStatus = GX_CP_REG_READ_U16(CP_STATUS);
    *overhi = (u8)CP_REG_STATUS_GET_OVFL(__GXData->cpStatus);
    *underlow = (u8)CP_REG_STATUS_GET_UNFL(__GXData->cpStatus);
    *readIdle = (u8)CP_REG_STATUS_GET_FIFO_RDIDLE(__GXData->cpStatus);
    *cmdIdle = (u8)CP_REG_STATUS_GET_CPIDLE(__GXData->cpStatus);
    *brkpt = (u8)CP_REG_STATUS_GET_FIFOBRK(__GXData->cpStatus);
}

/*
 * GXGetCPUFifo
 *
 * Copies the current CPU FIFO state into fifo; returns FALSE if no CPU
 * FIFO is set.
 */
// 0x802391F0
BOOL GXGetCPUFifo(GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    if (!CPUFifoReady) {
        return FALSE;
    }

    __GXSaveFifo();
    *realFifo = CPUFifo;
    return TRUE;
}

/*
 * GXGetGPFifo
 *
 * Copies the current GP FIFO state into fifo; returns FALSE if no GP FIFO
 * is set. Dead-stripped in retail.
 */
BOOL GXGetGPFifo(GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    if (!GPFifoReady) {
        return FALSE;
    }

    __GXSaveFifo();
    *realFifo = GPFifo;
    return TRUE;
}

/*
 * GXGetFifoPtrs
 *
 * Reads the read and write pointers of a FIFO object.
 */
// 0x80239280
void GXGetFifoPtrs(GXFifoObj* fifo, void** readPtr, void** writePtr) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;
    *readPtr = realFifo->rdPtr;
    *writePtr = realFifo->wrPtr;
}

/*
 * GXGetFifoBase
 *
 * Returns the base address of a FIFO object.
 */
// 0x802392A0
void* GXGetFifoBase(const GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;

    return realFifo->base;
}

/*
 * GXGetFifoSize
 *
 * Returns the size of a FIFO object. Dead-stripped in retail.
 */
u32 GXGetFifoSize(const GXFifoObj* fifo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;
    return realFifo->size;
}

/*
 * GXGetFifoLimits
 *
 * Reads the watermarks of a FIFO object. Dead-stripped in retail.
 */
void GXGetFifoLimits(const GXFifoObj* fifo, u32* hi, u32* lo) {
    __GXFifoObj* realFifo = (__GXFifoObj*)fifo;
    *hi = realFifo->hiWatermark;
    *lo = realFifo->loWatermark;
}

/*
 * GXSetBreakPtCallback
 *
 * Installs the FIFO breakpoint callback and returns the previous one.
 */
// 0x802392B0
GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb) {
    GXBreakPtCallback oldcb = BreakPointCB;
    BOOL enabled = OSDisableInterrupts();

    BreakPointCB = cb;
    OSRestoreInterrupts(enabled);
    return oldcb;
}

/*
 * GXEnableBreakPt
 *
 * Arms a FIFO breakpoint at the given address.
 */
// 0x80239300
void GXEnableBreakPt(void* break_pt) {
    BOOL enabled = OSDisableInterrupts();

    __GXFifoReadDisable();
    GX_CP_REG_WRITE_U16(CP_FIFO_BRKL, TOPHYSICAL(break_pt));
    GX_CP_REG_WRITE_U16(CP_FIFO_BRKH, TOPHYSICAL(break_pt) >> 16);
    SC_CP_REG_ENABLE_SET_FIFOBRK(0, __GXData->cpEnable, 0);
    SC_CP_REG_ENABLE_SET_FIFOBRKINT(0, __GXData->cpEnable, 0);
    GX_CP_REG_WRITE_U16(CP_ENABLE, __GXData->cpEnable);
    SC_CP_REG_ENABLE_SET_FIFOBRK(0, __GXData->cpEnable, 1);
    SC_CP_REG_ENABLE_SET_FIFOBRKINT(0, __GXData->cpEnable, 1);
    GX_CP_REG_WRITE_U16(CP_ENABLE, __GXData->cpEnable);
    __GXCurrentBP = break_pt;
    __GXFifoReadEnable();
    OSRestoreInterrupts(enabled);
}

/*
 * GXDisableBreakPt
 *
 * Disarms the FIFO breakpoint.
 */
// 0x802393A0
void GXDisableBreakPt(void) {
    BOOL enabled = OSDisableInterrupts();

    SC_CP_REG_ENABLE_SET_FIFOBRK(0, __GXData->cpEnable, 0);
    SC_CP_REG_ENABLE_SET_FIFOBRKINT(0, __GXData->cpEnable, 0);
    GX_CP_REG_WRITE_U16(CP_ENABLE, __GXData->cpEnable);
    __GXCurrentBP = NULL;
    OSRestoreInterrupts(enabled);
}

/*
 * __GXFifoInit
 *
 * Installs the CP interrupt handler and clears the CPU and GP FIFO state.
 */
// 0x802393F0
void __GXFifoInit(void) {
    __OSSetInterruptHandler(0x11, GXCPInterruptHandler);
    __OSUnmaskInterrupts(0x4000);  // OS_INTERRUPTMASK_PI_CP
    __GXCurrentThread = OSGetCurrentThread();
    GXOverflowSuspendInProgress = FALSE;
    memset(&CPUFifo, 0, sizeof(__GXFifoObj));
    memset(&GPFifo, 0, sizeof(__GXFifoObj));
    CPUFifoReady = GX_FALSE;
    GPFifoReady  = GX_FALSE;
}

/*
 * __GXFifoReadEnable
 *
 * Lets the command processor read the FIFO. Inlined into its callers in
 * retail.
 */
static void __GXFifoReadEnable(void) {
    SC_CP_REG_ENABLE_SET_FIFORD(0, __GXData->cpEnable, 1); 
    GX_CP_REG_WRITE_U16(CP_ENABLE, (u16)__GXData->cpEnable);
}

/*
 * __GXFifoReadDisable
 *
 * Stops the command processor reading the FIFO. Inlined into its callers
 * in retail.
 */
static void __GXFifoReadDisable(void) {
    SC_CP_REG_ENABLE_SET_FIFORD(0, __GXData->cpEnable, 0); 
    GX_CP_REG_WRITE_U16(CP_ENABLE, (u16)__GXData->cpEnable);
}

/*
 * __GXFifoLink
 *
 * Links or unlinks the CPU and GP FIFO write pointers. Inlined into its
 * callers in retail.
 */
static void __GXFifoLink(u8 en) {
    SC_CP_REG_ENABLE_SET_WRPTRINC(1385, __GXData->cpEnable, (en ? 0x1 : 0)); 
    GX_CP_REG_WRITE_U16(CP_ENABLE, (u16)__GXData->cpEnable);
}

/*
 * __GXWriteFifoIntEnable
 *
 * Enables or disables the high and low watermark interrupts. Inlined into
 * its callers in retail.
 */
static void __GXWriteFifoIntEnable(u8 hiWatermarkEn, u8 loWatermarkEn) {
    SC_CP_REG_ENABLE_SET_OVFLINT(1407, __GXData->cpEnable, hiWatermarkEn);
    SC_CP_REG_ENABLE_SET_UNFLINT(1408, __GXData->cpEnable, loWatermarkEn);
    GX_CP_REG_WRITE_U16(CP_ENABLE, (u16)__GXData->cpEnable);
}

/*
 * __GXWriteFifoIntReset
 *
 * Clears the high and low watermark interrupt flags. Inlined into its
 * callers in retail.
 */
static void __GXWriteFifoIntReset(u8 hiWatermarkClr, u8 loWatermarkClr) {
    SC_CP_REG_CLR_SET_OVFLINT(1431, __GXData->cpClr, hiWatermarkClr);
    SC_CP_REG_CLR_SET_UNFLINT(1432, __GXData->cpClr, loWatermarkClr);
    GX_CP_REG_WRITE_U16(CP_CLR, (u16)__GXData->cpClr);
}

/*
 * __GXCleanGPFifo
 *
 * Empties the GP FIFO by moving its read pointer to the write pointer.
 */
// 0x80239470
void __GXCleanGPFifo(void) {
    BOOL enabled;

    if ( !GPFifoReady) {
        return;
    }

    enabled = OSDisableInterrupts();
    __GXFifoReadDisable();
    __GXWriteFifoIntEnable(GX_FALSE, GX_FALSE);
    GPFifo.rdPtr = GPFifo.wrPtr;
    GPFifo.count = 0;
    GX_CP_REG_WRITE_U16(CP_FIFO_COUNTL, GPFifo.count);
    GX_CP_REG_WRITE_U16(CP_FIFO_WPTRL, TOPHYSICAL(GPFifo.wrPtr));
    GX_CP_REG_WRITE_U16(CP_FIFO_RPTRL, TOPHYSICAL(GPFifo.rdPtr));
    GX_CP_REG_WRITE_U16(CP_FIFO_COUNTH, GPFifo.count>>16);
    GX_CP_REG_WRITE_U16(CP_FIFO_WPTRH, TOPHYSICAL(GPFifo.wrPtr)>>16);
    GX_CP_REG_WRITE_U16(CP_FIFO_RPTRH, TOPHYSICAL(GPFifo.rdPtr)>>16);

    PPCSync();

    if (CPGPLinked) {
        u32 reg = 0;
        CPUFifo.rdPtr = GPFifo.rdPtr;
        CPUFifo.wrPtr = GPFifo.wrPtr;
        CPUFifo.count = GPFifo.count;
        SC_PI_CPWRT_REG_SET_CPWRT(1501, reg, (GX_PHY_ADDR(TOPHYSICAL(CPUFifo.wrPtr)) >> 5));
        SC_PI_CPWRT_REG_SET_CPWRAP(0, reg, 0);
        GX_PI_REG_WRITE_U32(PI_REG_CPWRT, reg);
        __GXWriteFifoIntEnable(GX_TRUE, GX_FALSE);
        __GXFifoLink(GX_TRUE);
    }

    SC_CP_REG_ENABLE_SET_FIFOBRK(0, __GXData->cpEnable, 0);
    SC_CP_REG_ENABLE_SET_FIFOBRKINT(0, __GXData->cpEnable, 0);
    GX_CP_REG_WRITE_U16(CP_ENABLE, __GXData->cpEnable);
    __GXCurrentBP = 0;
    __GXWriteFifoIntReset(GX_TRUE, GX_TRUE);
    __GXFifoReadEnable();
    OSRestoreInterrupts(enabled);
}

/*
 * GXSetCurrentGXThread
 *
 * Makes the calling thread the one suspended on FIFO overflow and returns
 * the previous one.
 */
// 0x80239610
OSThread* GXSetCurrentGXThread(void) {
    BOOL enabled;
    OSThread* prev;

    enabled = OSDisableInterrupts();
    prev = __GXCurrentThread;
    ASSERTMSG(!GXOverflowSuspendInProgress, "GXSetCurrentGXThread: Two threads cannot generate GX commands at the same time!");
    __GXCurrentThread = OSGetCurrentThread();
    OSRestoreInterrupts(enabled);
    return prev;
}

/*
 * GXGetCurrentGXThread
 *
 * Returns the thread suspended on FIFO overflow. Dead-stripped in retail.
 */
OSThread* GXGetCurrentGXThread(void) {
    return __GXCurrentThread;
}

/*
 * GXGetOverflowCount
 *
 * Returns the number of FIFO overflows. Dead-stripped in retail.
 */
u32 GXGetOverflowCount(void) {
    return __GXOverflowCount;
}

/*
 * GXResetOverflowCount
 *
 * Returns the number of FIFO overflows and resets it. Dead-stripped in
 * retail.
 */
u32 GXResetOverflowCount(void) {
    u32 oldcount;

    oldcount = __GXOverflowCount;
    __GXOverflowCount = 0;
    return oldcount;
}

/*
 * GXRedirectWriteGatherPipe
 *
 * Redirects the write-gather pipe to a buffer. Dead-stripped in retail.
 */
// volatile: the write-gather pipe port is a hardware address
volatile void* GXRedirectWriteGatherPipe(void* ptr) {
    u32 reg = 0;
    BOOL enabled = OSDisableInterrupts();

    CHECK_GXBEGIN("GXRedirectWriteGatherPipe");
    ASSERT(OFFSET(ptr, 32) == 0);
    ASSERT(!IsWGPipeRedirected);

#if DEBUG
    IsWGPipeRedirected = TRUE;
#endif

    GXFlush();
    while (PPCMfwpar() & 1) {}

    PPCMtwpar((u32)OSUncachedToPhysical((void*)GXFIFO_ADDR));

    if (CPGPLinked) {
        __GXFifoLink(0);
        __GXWriteFifoIntEnable(0, 0);
    }

    // PI CPWRT: 0xFBFFFFFF clears the wrap bit (bit 26); 0x3FFFFFFF is the physical-address mask
    CPUFifo.wrPtr = OSPhysicalToCached(GX_GET_PI_REG(5) & 0xFBFFFFFF);
    GX_SET_PI_REG(3, 0);
    GX_SET_PI_REG(4, 0x04000000);
    SET_REG_FIELD(__LINE__, reg, 21, 5, ((u32)ptr & 0x3FFFFFFF) >> 5);
    reg &= 0xFBFFFFFF;
    GX_SET_PI_REG(5, reg);

    PPCSync();
    OSRestoreInterrupts(enabled);
    return (volatile void *)GXFIFO_ADDR;  // volatile: write-gather pipe port
}

/*
 * GXRestoreWriteGatherPipe
 *
 * Points the write-gather pipe back at the CPU FIFO. Dead-stripped in
 * retail.
 */
void GXRestoreWriteGatherPipe(void) {
    u32 reg = 0;
    u32 i;
    BOOL enabled;

    ASSERT(IsWGPipeRedirected);

#if DEBUG
    IsWGPipeRedirected = FALSE;
#endif

    enabled = OSDisableInterrupts();
    for (i = 0; i < 31; i++) {
        GXWGFifo.u8 = 0;
    }

    PPCSync();
    while (PPCMfwpar() & 1) {}
    PPCMtwpar((u32)OSUncachedToPhysical((void *)GXFIFO_ADDR));
    // PI CPBAS/CPTOP/CPWRT take physical addresses (mask 0x3FFFFFFF); 0xFBFFFFFF clears the wrap bit
    GX_SET_PI_REG(3, (u32)CPUFifo.base & 0x3FFFFFFF);
    GX_SET_PI_REG(4, (u32)CPUFifo.top & 0x3FFFFFFF);
    SET_REG_FIELD(__LINE__, reg, 21, 5, ((u32)CPUFifo.wrPtr & 0x3FFFFFFF) >> 5);
    reg &= 0xFBFFFFFF;
    GX_SET_PI_REG(5, reg);
    if (CPGPLinked) {
        __GXWriteFifoIntReset(1, 1);
        __GXWriteFifoIntEnable(1, 0);
        __GXFifoLink(1);
    }

    PPCSync();
    OSRestoreInterrupts(enabled);
}
