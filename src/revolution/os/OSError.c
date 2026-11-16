#include <revolution/base.h>
#include <revolution/hardware.h>
#include <revolution/os.h>
#include <stdarg.h>
#include <stdio.h>

OSErrorHandler __OSErrorTable[OS_ERROR_MAX];

/* FPSCR exception enables that OSSetErrorHandler(OS_ERROR_FPE) applies to every thread. */
u32 __OSFpscrEnableBits = FPSCR_ENABLE;

/*
 * OSReport
 */
void OSReport(const char* msg, ...) {
    va_list marker;

    va_start(marker, msg);
    vprintf(msg, marker);
    va_end(marker);
} // 0x80213EA0

/*
 * OSPanic
 *
 * Prints the message with its source position and the back chain, then
 * halts.
 */
void OSPanic(const char* file, int line, const char* msg, ...) {
    va_list marker;
    u32 i;
    u32* p;

    OSDisableInterrupts();
    va_start(marker, msg);
    vprintf(msg, marker);
    va_end(marker);
    OSReport(" in \"%s\" on line %d.\n", file, line);

    OSReport("\nAddress:      Back Chain    LR Save\n");
    for (i = 0, p = (u32*)OSGetStackPointer(); p && (u32)p != 0xFFFFFFFF && i++ < 16; p = (u32*)*p) {
        OSReport("0x%08x:   0x%08x    0x%08x\n", p, p[0], p[1]);
    }

    PPCHalt();
} // 0x80213F20

/*
 * OSSetErrorHandler
 *
 * Installs handler for error and returns the previous one. Installing or
 * removing the FPE handler also turns the FP exception enables on or off
 * in every thread's saved context and in the running FPSCR.
 */
OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler) {
    OSErrorHandler oldHandler;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    oldHandler = __OSErrorTable[error];
    __OSErrorTable[error] = handler;

    if (error == OS_ERROR_FPE) {
        u32 msr;
        u32 fpscr;
        OSThread* thread;

        msr = PPCMfmsr();
        PPCMtmsr(msr | MSR_FP);
        fpscr = PPCMffpscr();
        if (handler) {
            for (thread = __OSActiveThreadQueue.head; thread; thread = thread->linkActive.next) {
                thread->context.srr1 |= MSR_FE0 | MSR_FE1;
                if (!(thread->context.state & OS_CONTEXT_STATE_FPSAVED)) {
                    int i;

                    /* Give a thread that never used the FPU a defined FP state to trap on. */
                    thread->context.state |= OS_CONTEXT_STATE_FPSAVED;
                    for (i = 0; i < 32; ++i) {
                        *(u64*)&thread->context.fpr[i] = 0xFFFFFFFFFFFFFFFFULL;
                        *(u64*)&thread->context.psf[i] = 0xFFFFFFFFFFFFFFFFULL;
                    }
                    thread->context.fpscr = FPSCR_NI;
                }
                thread->context.fpscr |= __OSFpscrEnableBits & FPSCR_ENABLE;
                thread->context.fpscr &= ~FPSCR_STICKY_EXCEPTIONS;
            }
            fpscr |= __OSFpscrEnableBits & FPSCR_ENABLE;
            msr |= MSR_FE0 | MSR_FE1;
        } else {
            for (thread = __OSActiveThreadQueue.head; thread; thread = thread->linkActive.next) {
                thread->context.srr1 &= ~(MSR_FE0 | MSR_FE1);
                thread->context.fpscr &= ~FPSCR_ENABLE;
                thread->context.fpscr &= ~FPSCR_STICKY_EXCEPTIONS;
            }
            fpscr &= ~FPSCR_ENABLE;
            msr &= ~(MSR_FE0 | MSR_FE1);
        }

        fpscr &= ~FPSCR_STICKY_EXCEPTIONS;
        PPCMtfpscr(fpscr);
        PPCMtmsr(msr);
    }

    OSRestoreInterrupts(enabled);
    return oldHandler;
} // 0x80214050

/*
 * __OSUnhandledException
 *
 * Dispatches an exception to its installed error handler, and otherwise
 * prints a crash report and halts. A program exception raised by an
 * enabled FP exception is reported as OS_ERROR_FPE.
 */
void __OSUnhandledException(__OSException exception, OSContext* context, u32 dsisr, u32 dar) {
    OSTime now;

    now = OSGetTime();

    if (!(context->srr1 & MSR_RI)) {
        OSReport("Non-recoverable Exception %d", exception);
    } else {
        if (exception == __OS_EXCEPTION_PROGRAM && (context->srr1 & SRR1_PROGRAM_FPE) &&
            __OSErrorTable[OS_ERROR_FPE] != NULL) {
            u32 fpscr;
            u32 msr;

            exception = OS_ERROR_FPE;

            msr = PPCMfmsr();
            PPCMtmsr(msr | MSR_FP);
            if (__OSFPUContext) {
                OSSaveFPUContext(__OSFPUContext);
            }
            fpscr = PPCMffpscr();
            fpscr &= ~FPSCR_STICKY_EXCEPTIONS;
            PPCMtfpscr(fpscr);
            PPCMtmsr(msr);

            if (__OSFPUContext == context) {
                OSDisableScheduler();
                __OSErrorTable[exception](exception, context, dsisr, dar);
                context->srr1 &= ~MSR_FP;
                __OSFPUContext = NULL;
                context->fpscr &= ~FPSCR_STICKY_EXCEPTIONS;
                OSEnableScheduler();
                __OSReschedule();
            } else {
                context->srr1 &= ~MSR_FP;
                __OSFPUContext = NULL;
            }
            OSLoadContext(context);
        }

        if (__OSErrorTable[exception]) {
            OSDisableScheduler();
            __OSErrorTable[exception](exception, context, dsisr, dar);
            OSEnableScheduler();
            __OSReschedule();
            OSLoadContext(context);
        }

        if (exception == OS_ERROR_DECREMENTER) {
            OSLoadContext(context);
        }

        OSReport("Unhandled Exception %d", exception);
    }

    OSReport("\n");
    OSDumpContext(context);
    OSReport("\nDSISR = 0x%08x                   DAR  = 0x%08x\n", dsisr, dar);
    OSReport("TB = 0x%016llx\n", now);

    switch (exception) {
    case __OS_EXCEPTION_DSI:
        OSReport("\nInstruction at 0x%x (read from SRR0) attempted to access invalid address 0x%x (read from DAR)\n",
                 context->srr0, dar);
        break;
    case __OS_EXCEPTION_ISI:
        OSReport("\nAttempted to fetch instruction from invalid address 0x%x (read from SRR0)\n", context->srr0);
        break;
    case __OS_EXCEPTION_ALIGNMENT:
        OSReport("\nInstruction at 0x%x (read from SRR0) attempted to access unaligned address 0x%x (read from DAR)\n",
                 context->srr0, dar);
        break;
    case __OS_EXCEPTION_PROGRAM:
        OSReport("\nProgram exception : Possible illegal instruction/operation at or around 0x%x (read from SRR0)\n",
                 context->srr0, dar);
        break;
    case OS_ERROR_PROTECTION:
        OSReport("\n");
        OSReport("AI DMA Address =   0x%04x%04x\n", __DSPRegs[0x18], __DSPRegs[0x19]);
        OSReport("ARAM DMA Address = 0x%04x%04x\n", __DSPRegs[0x10], __DSPRegs[0x11]);
        OSReport("DI DMA Address =   0x%08x\n", __DIRegs[5]);
        break;
    }

    OSReport("\nLast interrupt (%d): SRR0 = 0x%08x  TB = 0x%016llx\n", __OSLastInterrupt, __OSLastInterruptSrr0,
             __OSLastInterruptTime);
    PPCHalt();
} // 0x802142E0
