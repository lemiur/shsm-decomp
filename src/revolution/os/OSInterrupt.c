#include <revolution/hardware.h>
#include <revolution/os.h>
#include <string.h>

/* Low-memory words holding the masks: the OS's own (__OSMaskInterrupts) and the application's. */
#define OS_INTERRUPTMASK_ADDR      0x00C4
#define OS_USER_INTERRUPTMASK_ADDR 0x00C8

/* Low-memory table of the external interrupt handlers. */
#define OS_INTERRUPT_TABLE_ADDR 0x3040

/* Hollywood interrupt mask for the PPC and its IPC bit; names invented. */
#define ACR_PPC_INTMSK (0x34 / 4)
#define ACR_INTMSK_IPC 0x40000000

/* PI interrupt cause (PI_INTSR) and mask (PI_INTMSK) registers and their bits. */
#define PI_INTSR         0
#define PI_INTMSK        1
#define PI_INT_ERROR     0x0001
#define PI_INT_RSW       0x0002
#define PI_INT_DI        0x0004
#define PI_INT_SI        0x0008
#define PI_INT_EXI       0x0010
#define PI_INT_AI        0x0020
#define PI_INT_DSP       0x0040
#define PI_INT_MEM       0x0080
#define PI_INT_VI        0x0100
#define PI_INT_PE_TOKEN  0x0200
#define PI_INT_PE_FINISH 0x0400
#define PI_INT_CP        0x0800
#define PI_INT_DEBUG     0x1000
#define PI_INT_HSP       0x2000
#define PI_INT_ACR       0x4000
#define PI_INTSR_RSWST   0x10000

static __OSInterruptHandler* InterruptHandlerTable;

volatile OSTime __OSLastInterruptTime;    // volatile: SDK OSInterrupt.h; written by the interrupt handler
volatile __OSInterrupt __OSLastInterrupt; // volatile: SDK OSInterrupt.h; written by the interrupt handler
volatile u32 __OSLastInterruptSrr0;       // volatile: SDK OSInterrupt.h; written by the interrupt handler

/* Order in which simultaneous interrupts are dispatched. */
static OSInterruptMask InterruptPrioTable[] = {
    OS_INTERRUPTMASK_PI_ERROR,
    OS_INTERRUPTMASK_PI_DEBUG,
    OS_INTERRUPTMASK_MEM,
    OS_INTERRUPTMASK_PI_RSW,
    OS_INTERRUPTMASK_PI_VI,
    OS_INTERRUPTMASK_PI_ACR,
    OS_INTERRUPTMASK_PI_PE_TOKEN | OS_INTERRUPTMASK_PI_PE_FINISH,
    OS_INTERRUPTMASK_PI_HSP,
    OS_INTERRUPTMASK_DSP_ARAM | OS_INTERRUPTMASK_DSP_DSP | OS_INTERRUPTMASK_AI | OS_INTERRUPTMASK_EXI |
        OS_INTERRUPTMASK_PI_SI | OS_INTERRUPTMASK_PI_DI,
    OS_INTERRUPTMASK_DSP_AI,
    OS_INTERRUPTMASK_PI_CP,
    0xFFFFFFFF,
};

/*
 * OSDisableInterrupts
 *
 * Clears MSR[EE] and returns its previous state. OSLoadContext restarts a
 * thread interrupted between the RAS labels from the beginning.
 */
// ORIGINAL-ASM: MSR access with entry labels; an asm function in the public RVL SDK OSInterrupt.c
asm BOOL OSDisableInterrupts(void) {
    nofralloc
entry __RAS_OSDisableInterrupts_begin
    mfmsr r3
    rlwinm r4, r3, 0, 17, 15
    mtmsr r4
entry __RAS_OSDisableInterrupts_end
    rlwinm r3, r3, 17, 31, 31
    blr
} // 0x802174E0

/*
 * OSEnableInterrupts
 *
 * Sets MSR[EE] and returns its previous state.
 */
// ORIGINAL-ASM: MSR access; an asm function in the public RVL SDK OSInterrupt.c
asm BOOL OSEnableInterrupts(void) {
    nofralloc
    mfmsr r3
    ori r4, r3, 0x8000
    mtmsr r4
    rlwinm r3, r3, 17, 31, 31
    blr
} // 0x80217500

/*
 * OSRestoreInterrupts
 *
 * Sets MSR[EE] to level and returns its previous state.
 */
// ORIGINAL-ASM: MSR access; an asm function in the public RVL SDK OSInterrupt.c
asm BOOL OSRestoreInterrupts(register BOOL level) {
    nofralloc
    cmpwi level, 0
    mfmsr r4
    beq _disable
    ori r5, r4, 0x8000
    b _restore
_disable:
    rlwinm r5, r4, 0, 17, 15
_restore:
    mtmsr r5
    rlwinm r3, r4, 17, 31, 31
    blr
} // 0x80217520

/*
 * __OSSetInterruptHandler
 *
 * Installs handler for interrupt and returns the previous one.
 */
__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler) {
    __OSInterruptHandler oldHandler;

    oldHandler = InterruptHandlerTable[interrupt];
    InterruptHandlerTable[interrupt] = handler;
    return oldHandler;
} // 0x80217550

__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt) {
    return InterruptHandlerTable[interrupt];
} // 0x80217570

static void ExternalInterruptHandler(register __OSException exception, register OSContext* context);

/*
 * __OSInterruptInit
 *
 * Clears the handler table and both masks, then masks every source and
 * installs the external interrupt exception handler.
 */
void __OSInterruptInit(void) {
    InterruptHandlerTable = OSPhysicalToCached(OS_INTERRUPT_TABLE_ADDR);
    memset(InterruptHandlerTable, 0, __OS_INTERRUPT_MAX * sizeof(__OSInterruptHandler));

    *(OSInterruptMask*)OSPhysicalToCached(OS_INTERRUPTMASK_ADDR) = 0;
    *(OSInterruptMask*)OSPhysicalToCached(OS_USER_INTERRUPTMASK_ADDR) = 0;

    __PIRegs[PI_INTMSK] = 0xF0;
    __ACRRestrictedRegs[ACR_PPC_INTMSK] = ACR_INTMSK_IPC;

    __OSMaskInterrupts(OS_INTERRUPTMASK_MEM | OS_INTERRUPTMASK_DSP | OS_INTERRUPTMASK_AI | OS_INTERRUPTMASK_EXI |
                       OS_INTERRUPTMASK_PI);
    __OSSetExceptionHandler(__OS_EXCEPTION_EXTERNAL_INTERRUPT, ExternalInterruptHandler);
} // 0x80217580

/*
 * SetInterruptMask
 *
 * Programs the hardware mask of the device that owns the highest source in
 * mask from current (a set bit masks the source), and returns mask without
 * that device's sources.
 */
static u32 SetInterruptMask(OSInterruptMask mask, OSInterruptMask current) {
    u32 reg;

    switch (__cntlzw(mask)) {
    case __OS_INTERRUPT_MEM_0:
    case __OS_INTERRUPT_MEM_1:
    case __OS_INTERRUPT_MEM_2:
    case __OS_INTERRUPT_MEM_3:
    case __OS_INTERRUPT_MEM_ADDRESS:
        reg = 0;
        if (!(current & OS_INTERRUPTMASK_MEM_0)) {
            reg |= 0x1;
        }
        if (!(current & OS_INTERRUPTMASK_MEM_1)) {
            reg |= 0x2;
        }
        if (!(current & OS_INTERRUPTMASK_MEM_2)) {
            reg |= 0x4;
        }
        if (!(current & OS_INTERRUPTMASK_MEM_3)) {
            reg |= 0x8;
        }
        if (!(current & OS_INTERRUPTMASK_MEM_ADDRESS)) {
            reg |= 0x10;
        }
        __MEMRegs[14] = (u16)reg;
        mask &= ~OS_INTERRUPTMASK_MEM;
        break;
    case __OS_INTERRUPT_DSP_AI:
    case __OS_INTERRUPT_DSP_ARAM:
    case __OS_INTERRUPT_DSP_DSP:
        reg = __DSPRegs[5];
        reg &= ~0x1F8;
        if (!(current & OS_INTERRUPTMASK_DSP_AI)) {
            reg |= 0x10;
        }
        if (!(current & OS_INTERRUPTMASK_DSP_ARAM)) {
            reg |= 0x40;
        }
        if (!(current & OS_INTERRUPTMASK_DSP_DSP)) {
            reg |= 0x100;
        }
        __DSPRegs[5] = (u16)reg;
        mask &= ~OS_INTERRUPTMASK_DSP;
        break;
    case __OS_INTERRUPT_AI_AI:
        reg = __AIRegs[0];
        reg &= ~0x2C;
        if (!(current & OS_INTERRUPTMASK_AI_AI)) {
            reg |= 0x4;
        }
        __AIRegs[0] = reg;
        mask &= ~OS_INTERRUPTMASK_AI;
        break;
    case __OS_INTERRUPT_EXI_0_EXI:
    case __OS_INTERRUPT_EXI_0_TC:
    case __OS_INTERRUPT_EXI_0_EXT:
        reg = __EXIRegs[0];
        reg &= ~0x2C0F; // the masks, the write-1-to-clear status bits and channel 0's ROMDIS
        if (!(current & OS_INTERRUPTMASK_EXI_0_EXI)) {
            reg |= 0x1;
        }
        if (!(current & OS_INTERRUPTMASK_EXI_0_TC)) {
            reg |= 0x4;
        }
        if (!(current & OS_INTERRUPTMASK_EXI_0_EXT)) {
            reg |= 0x400;
        }
        __EXIRegs[0] = reg;
        mask &= ~OS_INTERRUPTMASK_EXI_0;
        break;
    case __OS_INTERRUPT_EXI_1_EXI:
    case __OS_INTERRUPT_EXI_1_TC:
    case __OS_INTERRUPT_EXI_1_EXT:
        reg = __EXIRegs[5];
        reg &= ~0xC0F;
        if (!(current & OS_INTERRUPTMASK_EXI_1_EXI)) {
            reg |= 0x1;
        }
        if (!(current & OS_INTERRUPTMASK_EXI_1_TC)) {
            reg |= 0x4;
        }
        if (!(current & OS_INTERRUPTMASK_EXI_1_EXT)) {
            reg |= 0x400;
        }
        __EXIRegs[5] = reg;
        mask &= ~OS_INTERRUPTMASK_EXI_1;
        break;
    case __OS_INTERRUPT_EXI_2_EXI:
    case __OS_INTERRUPT_EXI_2_TC:
        reg = __EXIRegs[10];
        reg &= ~0xF;
        if (!(current & OS_INTERRUPTMASK_EXI_2_EXI)) {
            reg |= 0x1;
        }
        if (!(current & OS_INTERRUPTMASK_EXI_2_TC)) {
            reg |= 0x4;
        }
        __EXIRegs[10] = reg;
        mask &= ~OS_INTERRUPTMASK_EXI_2;
        break;
    case __OS_INTERRUPT_PI_CP:
    case __OS_INTERRUPT_PI_PE_TOKEN:
    case __OS_INTERRUPT_PI_PE_FINISH:
    case __OS_INTERRUPT_PI_SI:
    case __OS_INTERRUPT_PI_DI:
    case __OS_INTERRUPT_PI_RSW:
    case __OS_INTERRUPT_PI_ERROR:
    case __OS_INTERRUPT_PI_VI:
    case __OS_INTERRUPT_PI_DEBUG:
    case __OS_INTERRUPT_PI_HSP:
    case __OS_INTERRUPT_PI_ACR:
        reg = 0xF0;
        if (!(current & OS_INTERRUPTMASK_PI_CP)) {
            reg |= PI_INT_CP;
        }
        if (!(current & OS_INTERRUPTMASK_PI_SI)) {
            reg |= PI_INT_SI;
        }
        if (!(current & OS_INTERRUPTMASK_PI_DI)) {
            reg |= PI_INT_DI;
        }
        if (!(current & OS_INTERRUPTMASK_PI_RSW)) {
            reg |= PI_INT_RSW;
        }
        if (!(current & OS_INTERRUPTMASK_PI_ERROR)) {
            reg |= PI_INT_ERROR;
        }
        if (!(current & OS_INTERRUPTMASK_PI_VI)) {
            reg |= PI_INT_VI;
        }
        if (!(current & OS_INTERRUPTMASK_PI_DEBUG)) {
            reg |= PI_INT_DEBUG;
        }
        if (!(current & OS_INTERRUPTMASK_PI_PE_TOKEN)) {
            reg |= PI_INT_PE_TOKEN;
        }
        if (!(current & OS_INTERRUPTMASK_PI_PE_FINISH)) {
            reg |= PI_INT_PE_FINISH;
        }
        if (!(current & OS_INTERRUPTMASK_PI_HSP)) {
            reg |= PI_INT_HSP;
        }
        if (!(current & OS_INTERRUPTMASK_PI_ACR)) {
            reg |= PI_INT_ACR;
        }
        __PIRegs[PI_INTMSK] = reg;
        mask &= ~OS_INTERRUPTMASK_PI;
        break;
    default:
        break;
    }
    return mask;
} // 0x80217650

/*
 * __OSMaskInterrupts
 *
 * Adds global to the OS mask and reprograms each affected device. Returns
 * the previous OS mask.
 */
OSInterruptMask __OSMaskInterrupts(OSInterruptMask global) {
    BOOL enabled;
    OSInterruptMask prev;
    OSInterruptMask local;
    OSInterruptMask mask;

    enabled = OSDisableInterrupts();
    prev = *(OSInterruptMask*)OSPhysicalToCached(OS_INTERRUPTMASK_ADDR);
    local = *(OSInterruptMask*)OSPhysicalToCached(OS_USER_INTERRUPTMASK_ADDR);
    mask = ~(prev | local) & global;
    global |= prev;
    *(OSInterruptMask*)OSPhysicalToCached(OS_INTERRUPTMASK_ADDR) = global;
    while (mask) {
        mask = SetInterruptMask(mask, global | local);
    }
    OSRestoreInterrupts(enabled);
    return prev;
} // 0x802178B0

/*
 * __OSUnmaskInterrupts
 *
 * Removes global from the OS mask and reprograms each affected device.
 * Returns the previous OS mask.
 */
OSInterruptMask __OSUnmaskInterrupts(OSInterruptMask global) {
    BOOL enabled;
    OSInterruptMask prev;
    OSInterruptMask local;
    OSInterruptMask mask;

    enabled = OSDisableInterrupts();
    prev = *(OSInterruptMask*)OSPhysicalToCached(OS_INTERRUPTMASK_ADDR);
    local = *(OSInterruptMask*)OSPhysicalToCached(OS_USER_INTERRUPTMASK_ADDR);
    mask = (prev | local) & global;
    global = prev & ~global;
    *(OSInterruptMask*)OSPhysicalToCached(OS_INTERRUPTMASK_ADDR) = global;
    while (mask) {
        mask = SetInterruptMask(mask, global | local);
    }
    OSRestoreInterrupts(enabled);
    return prev;
} // 0x80217930

/*
 * __OSDispatchInterrupt
 *
 * Second level of the external interrupt handler: turns the PI cause and
 * the device status registers into an interrupt mask, calls the handler of
 * the highest-priority unmasked source and resumes the interrupted context.
 */
static void __OSDispatchInterrupt(__OSException exception, OSContext* context) {
    u32 intsr;
    u32 intmr;
    u32 reg;
    OSInterruptMask cause;
    OSInterruptMask unmasked;
    OSInterruptMask* prio;
    __OSInterruptHandler handler;
    __OSInterrupt interrupt;

    intsr = __PIRegs[PI_INTSR];
    intsr &= ~PI_INTSR_RSWST;
    intmr = __PIRegs[PI_INTMSK];
    if (intsr == 0 || (intsr & intmr) == 0) {
        OSLoadContext(context);
    }

    cause = 0;

    if (intsr & PI_INT_MEM) {
        reg = __MEMRegs[15];
        if (reg & 0x1) {
            cause |= OS_INTERRUPTMASK_MEM_0;
        }
        if (reg & 0x2) {
            cause |= OS_INTERRUPTMASK_MEM_1;
        }
        if (reg & 0x4) {
            cause |= OS_INTERRUPTMASK_MEM_2;
        }
        if (reg & 0x8) {
            cause |= OS_INTERRUPTMASK_MEM_3;
        }
        if (reg & 0x10) {
            cause |= OS_INTERRUPTMASK_MEM_ADDRESS;
        }
    }

    if (intsr & PI_INT_DSP) {
        reg = __DSPRegs[5];
        if (reg & 0x8) {
            cause |= OS_INTERRUPTMASK_DSP_AI;
        }
        if (reg & 0x20) {
            cause |= OS_INTERRUPTMASK_DSP_ARAM;
        }
        if (reg & 0x80) {
            cause |= OS_INTERRUPTMASK_DSP_DSP;
        }
    }

    if (intsr & PI_INT_AI) {
        reg = __AIRegs[0];
        if (reg & 0x8) {
            cause |= OS_INTERRUPTMASK_AI_AI;
        }
    }

    if (intsr & PI_INT_EXI) {
        reg = __EXIRegs[0];
        if (reg & 0x2) {
            cause |= OS_INTERRUPTMASK_EXI_0_EXI;
        }
        if (reg & 0x8) {
            cause |= OS_INTERRUPTMASK_EXI_0_TC;
        }
        if (reg & 0x800) {
            cause |= OS_INTERRUPTMASK_EXI_0_EXT;
        }
        reg = __EXIRegs[5];
        if (reg & 0x2) {
            cause |= OS_INTERRUPTMASK_EXI_1_EXI;
        }
        if (reg & 0x8) {
            cause |= OS_INTERRUPTMASK_EXI_1_TC;
        }
        if (reg & 0x800) {
            cause |= OS_INTERRUPTMASK_EXI_1_EXT;
        }
        reg = __EXIRegs[10];
        if (reg & 0x2) {
            cause |= OS_INTERRUPTMASK_EXI_2_EXI;
        }
        if (reg & 0x8) {
            cause |= OS_INTERRUPTMASK_EXI_2_TC;
        }
    }

    if (intsr & PI_INT_HSP) {
        cause |= OS_INTERRUPTMASK_PI_HSP;
    }
    if (intsr & PI_INT_DEBUG) {
        cause |= OS_INTERRUPTMASK_PI_DEBUG;
    }
    if (intsr & PI_INT_PE_FINISH) {
        cause |= OS_INTERRUPTMASK_PI_PE_FINISH;
    }
    if (intsr & PI_INT_PE_TOKEN) {
        cause |= OS_INTERRUPTMASK_PI_PE_TOKEN;
    }
    if (intsr & PI_INT_VI) {
        cause |= OS_INTERRUPTMASK_PI_VI;
    }
    if (intsr & PI_INT_SI) {
        cause |= OS_INTERRUPTMASK_PI_SI;
    }
    if (intsr & PI_INT_DI) {
        cause |= OS_INTERRUPTMASK_PI_DI;
    }
    if (intsr & PI_INT_RSW) {
        cause |= OS_INTERRUPTMASK_PI_RSW;
    }
    if (intsr & PI_INT_CP) {
        cause |= OS_INTERRUPTMASK_PI_CP;
    }
    if (intsr & PI_INT_ERROR) {
        cause |= OS_INTERRUPTMASK_PI_ERROR;
    }
    if (intsr & PI_INT_ACR) {
        cause |= OS_INTERRUPTMASK_PI_ACR;
    }

    unmasked = cause & ~(*(OSInterruptMask*)OSPhysicalToCached(OS_INTERRUPTMASK_ADDR) |
                         *(OSInterruptMask*)OSPhysicalToCached(OS_USER_INTERRUPTMASK_ADDR));
    if (unmasked) {
        for (prio = InterruptPrioTable;; ++prio) {
            if (unmasked & *prio) {
                interrupt = (__OSInterrupt)__cntlzw(unmasked & *prio);
                break;
            }
        }

        handler = InterruptHandlerTable[interrupt];
        if (handler) {
            if (__OS_INTERRUPT_MEM_ADDRESS < interrupt) {
                __OSLastInterrupt = interrupt;
                __OSLastInterruptTime = OSGetTime();
                __OSLastInterruptSrr0 = context->srr0;
            }

            OSDisableScheduler();
            handler(interrupt, context);
            OSEnableScheduler();
            __OSReschedule();
            OSLoadContext(context);
        }
    }

    OSLoadContext(context);
} // 0x802179B0

/*
 * ExternalInterruptHandler
 *
 * First-level external interrupt exception handler: saves the registers the
 * exception prologue has not, and branches to __OSDispatchInterrupt.
 */
// ORIGINAL-ASM: first-level exception stub; an asm function in the public RVL SDK OSInterrupt.c
static asm void ExternalInterruptHandler(register __OSException exception, register OSContext* context) {
    nofralloc
    stw r0, OS_CONTEXT_R0(context)
    stw r1, OS_CONTEXT_R1(context)
    stw r2, OS_CONTEXT_R2(context)
    stmw r6, OS_CONTEXT_R6(context)
    mfspr r0, GQR1
    stw r0, OS_CONTEXT_GQR1(context)
    mfspr r0, GQR2
    stw r0, OS_CONTEXT_GQR2(context)
    mfspr r0, GQR3
    stw r0, OS_CONTEXT_GQR3(context)
    mfspr r0, GQR4
    stw r0, OS_CONTEXT_GQR4(context)
    mfspr r0, GQR5
    stw r0, OS_CONTEXT_GQR5(context)
    mfspr r0, GQR6
    stw r0, OS_CONTEXT_GQR6(context)
    mfspr r0, GQR7
    stw r0, OS_CONTEXT_GQR7(context)
    stwu r1, -8(r1)
    b __OSDispatchInterrupt
} // 0x80217C60
