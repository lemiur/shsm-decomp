#include <revolution/base.h>
#include <revolution/db.h>
#include <revolution/os.h>

/*
 * __OSLoadFPUContext
 *
 * Restores the FPRs, the FPSCR and, with paired singles enabled, the PS1
 * halves from context, if the context has saved them.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
static asm void __OSLoadFPUContext(register u32 unused, register OSContext* fpuContext) {
    nofralloc
    lhz r5, fpuContext->state
    clrlwi. r5, r5, 31
    beq _return

    lfd f0, OS_CONTEXT_FPSCR_PAD(fpuContext)
    mtfsf 0xFF, f0
    mfspr r5, SPR_HID2
    rlwinm. r5, r5, 3, 31, 31   // HID2[PSE]
    beq _regular_FPRs

    psq_l f0, OS_CONTEXT_PSF0(fpuContext), 0, 0
    psq_l f1, OS_CONTEXT_PSF1(fpuContext), 0, 0
    psq_l f2, OS_CONTEXT_PSF2(fpuContext), 0, 0
    psq_l f3, OS_CONTEXT_PSF3(fpuContext), 0, 0
    psq_l f4, OS_CONTEXT_PSF4(fpuContext), 0, 0
    psq_l f5, OS_CONTEXT_PSF5(fpuContext), 0, 0
    psq_l f6, OS_CONTEXT_PSF6(fpuContext), 0, 0
    psq_l f7, OS_CONTEXT_PSF7(fpuContext), 0, 0
    psq_l f8, OS_CONTEXT_PSF8(fpuContext), 0, 0
    psq_l f9, OS_CONTEXT_PSF9(fpuContext), 0, 0
    psq_l f10, OS_CONTEXT_PSF10(fpuContext), 0, 0
    psq_l f11, OS_CONTEXT_PSF11(fpuContext), 0, 0
    psq_l f12, OS_CONTEXT_PSF12(fpuContext), 0, 0
    psq_l f13, OS_CONTEXT_PSF13(fpuContext), 0, 0
    psq_l f14, OS_CONTEXT_PSF14(fpuContext), 0, 0
    psq_l f15, OS_CONTEXT_PSF15(fpuContext), 0, 0
    psq_l f16, OS_CONTEXT_PSF16(fpuContext), 0, 0
    psq_l f17, OS_CONTEXT_PSF17(fpuContext), 0, 0
    psq_l f18, OS_CONTEXT_PSF18(fpuContext), 0, 0
    psq_l f19, OS_CONTEXT_PSF19(fpuContext), 0, 0
    psq_l f20, OS_CONTEXT_PSF20(fpuContext), 0, 0
    psq_l f21, OS_CONTEXT_PSF21(fpuContext), 0, 0
    psq_l f22, OS_CONTEXT_PSF22(fpuContext), 0, 0
    psq_l f23, OS_CONTEXT_PSF23(fpuContext), 0, 0
    psq_l f24, OS_CONTEXT_PSF24(fpuContext), 0, 0
    psq_l f25, OS_CONTEXT_PSF25(fpuContext), 0, 0
    psq_l f26, OS_CONTEXT_PSF26(fpuContext), 0, 0
    psq_l f27, OS_CONTEXT_PSF27(fpuContext), 0, 0
    psq_l f28, OS_CONTEXT_PSF28(fpuContext), 0, 0
    psq_l f29, OS_CONTEXT_PSF29(fpuContext), 0, 0
    psq_l f30, OS_CONTEXT_PSF30(fpuContext), 0, 0
    psq_l f31, OS_CONTEXT_PSF31(fpuContext), 0, 0

_regular_FPRs:
    lfd f0, fpuContext->fpr[0]
    lfd f1, fpuContext->fpr[1]
    lfd f2, fpuContext->fpr[2]
    lfd f3, fpuContext->fpr[3]
    lfd f4, fpuContext->fpr[4]
    lfd f5, fpuContext->fpr[5]
    lfd f6, fpuContext->fpr[6]
    lfd f7, fpuContext->fpr[7]
    lfd f8, fpuContext->fpr[8]
    lfd f9, fpuContext->fpr[9]
    lfd f10, fpuContext->fpr[10]
    lfd f11, fpuContext->fpr[11]
    lfd f12, fpuContext->fpr[12]
    lfd f13, fpuContext->fpr[13]
    lfd f14, fpuContext->fpr[14]
    lfd f15, fpuContext->fpr[15]
    lfd f16, fpuContext->fpr[16]
    lfd f17, fpuContext->fpr[17]
    lfd f18, fpuContext->fpr[18]
    lfd f19, fpuContext->fpr[19]
    lfd f20, fpuContext->fpr[20]
    lfd f21, fpuContext->fpr[21]
    lfd f22, fpuContext->fpr[22]
    lfd f23, fpuContext->fpr[23]
    lfd f24, fpuContext->fpr[24]
    lfd f25, fpuContext->fpr[25]
    lfd f26, fpuContext->fpr[26]
    lfd f27, fpuContext->fpr[27]
    lfd f28, fpuContext->fpr[28]
    lfd f29, fpuContext->fpr[29]
    lfd f30, fpuContext->fpr[30]
    lfd f31, fpuContext->fpr[31]

_return:
    blr
} // 0x802135C0

/*
 * __OSSaveFPUContext
 *
 * Saves the FPRs, the FPSCR and, with paired singles enabled, the PS1
 * halves into context, and marks them saved.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
static asm void __OSSaveFPUContext(register u32 unused1, register u32 unused2, register OSContext* fpuContext) {
    nofralloc
    lhz r3, fpuContext->state
    ori r3, r3, OS_CONTEXT_STATE_FPSAVED
    sth r3, fpuContext->state

    stfd f0, fpuContext->fpr[0]
    stfd f1, fpuContext->fpr[1]
    stfd f2, fpuContext->fpr[2]
    stfd f3, fpuContext->fpr[3]
    stfd f4, fpuContext->fpr[4]
    stfd f5, fpuContext->fpr[5]
    stfd f6, fpuContext->fpr[6]
    stfd f7, fpuContext->fpr[7]
    stfd f8, fpuContext->fpr[8]
    stfd f9, fpuContext->fpr[9]
    stfd f10, fpuContext->fpr[10]
    stfd f11, fpuContext->fpr[11]
    stfd f12, fpuContext->fpr[12]
    stfd f13, fpuContext->fpr[13]
    stfd f14, fpuContext->fpr[14]
    stfd f15, fpuContext->fpr[15]
    stfd f16, fpuContext->fpr[16]
    stfd f17, fpuContext->fpr[17]
    stfd f18, fpuContext->fpr[18]
    stfd f19, fpuContext->fpr[19]
    stfd f20, fpuContext->fpr[20]
    stfd f21, fpuContext->fpr[21]
    stfd f22, fpuContext->fpr[22]
    stfd f23, fpuContext->fpr[23]
    stfd f24, fpuContext->fpr[24]
    stfd f25, fpuContext->fpr[25]
    stfd f26, fpuContext->fpr[26]
    stfd f27, fpuContext->fpr[27]
    stfd f28, fpuContext->fpr[28]
    stfd f29, fpuContext->fpr[29]
    stfd f30, fpuContext->fpr[30]
    stfd f31, fpuContext->fpr[31]

    mffs f0
    stfd f0, OS_CONTEXT_FPSCR_PAD(fpuContext)
    lfd f0, fpuContext->fpr[0]

    mfspr r3, SPR_HID2
    rlwinm. r3, r3, 3, 31, 31   // HID2[PSE]
    beq _return

    psq_st f0, OS_CONTEXT_PSF0(fpuContext), 0, 0
    psq_st f1, OS_CONTEXT_PSF1(fpuContext), 0, 0
    psq_st f2, OS_CONTEXT_PSF2(fpuContext), 0, 0
    psq_st f3, OS_CONTEXT_PSF3(fpuContext), 0, 0
    psq_st f4, OS_CONTEXT_PSF4(fpuContext), 0, 0
    psq_st f5, OS_CONTEXT_PSF5(fpuContext), 0, 0
    psq_st f6, OS_CONTEXT_PSF6(fpuContext), 0, 0
    psq_st f7, OS_CONTEXT_PSF7(fpuContext), 0, 0
    psq_st f8, OS_CONTEXT_PSF8(fpuContext), 0, 0
    psq_st f9, OS_CONTEXT_PSF9(fpuContext), 0, 0
    psq_st f10, OS_CONTEXT_PSF10(fpuContext), 0, 0
    psq_st f11, OS_CONTEXT_PSF11(fpuContext), 0, 0
    psq_st f12, OS_CONTEXT_PSF12(fpuContext), 0, 0
    psq_st f13, OS_CONTEXT_PSF13(fpuContext), 0, 0
    psq_st f14, OS_CONTEXT_PSF14(fpuContext), 0, 0
    psq_st f15, OS_CONTEXT_PSF15(fpuContext), 0, 0
    psq_st f16, OS_CONTEXT_PSF16(fpuContext), 0, 0
    psq_st f17, OS_CONTEXT_PSF17(fpuContext), 0, 0
    psq_st f18, OS_CONTEXT_PSF18(fpuContext), 0, 0
    psq_st f19, OS_CONTEXT_PSF19(fpuContext), 0, 0
    psq_st f20, OS_CONTEXT_PSF20(fpuContext), 0, 0
    psq_st f21, OS_CONTEXT_PSF21(fpuContext), 0, 0
    psq_st f22, OS_CONTEXT_PSF22(fpuContext), 0, 0
    psq_st f23, OS_CONTEXT_PSF23(fpuContext), 0, 0
    psq_st f24, OS_CONTEXT_PSF24(fpuContext), 0, 0
    psq_st f25, OS_CONTEXT_PSF25(fpuContext), 0, 0
    psq_st f26, OS_CONTEXT_PSF26(fpuContext), 0, 0
    psq_st f27, OS_CONTEXT_PSF27(fpuContext), 0, 0
    psq_st f28, OS_CONTEXT_PSF28(fpuContext), 0, 0
    psq_st f29, OS_CONTEXT_PSF29(fpuContext), 0, 0
    psq_st f30, OS_CONTEXT_PSF30(fpuContext), 0, 0
    psq_st f31, OS_CONTEXT_PSF31(fpuContext), 0, 0

_return:
    blr
} // 0x802136F0

// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm void OSSaveFPUContext(register OSContext* fpuContext) {
    nofralloc
    addi r5, fpuContext, 0
    b __OSSaveFPUContext
} // 0x80213820

/*
 * OSSetCurrentContext
 *
 * Makes context the current context and sets MSR[FP] according to
 * whether it owns the FPU.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm void OSSetCurrentContext(register OSContext* context) {
    nofralloc
    lis r4, OS_CURRENT_CONTEXT_ADDR@ha
    stw context, OS_CURRENT_CONTEXT_ADDR@l(r4)

    clrlwi r5, context, 2
    stw r5, OS_CURRENT_CONTEXT_PHYS_ADDR@l(r4)

    lwz r5, OS_CURRENT_FPU_CONTEXT_ADDR@l(r4)
    cmpw r5, context
    bne _disableFPU

    lwz r6, context->srr1
    ori r6, r6, 0x2000      // MSR[FP]
    stw r6, context->srr1
    mfmsr r6
    ori r6, r6, 0x2         // MSR[RI]
    mtmsr r6
    blr

_disableFPU:
    lwz r6, context->srr1
    rlwinm r6, r6, 0, 19, 17
    stw r6, context->srr1
    mfmsr r6
    rlwinm r6, r6, 0, 19, 17
    ori r6, r6, 0x2         // MSR[RI]
    mtmsr r6
    isync
    blr
} // 0x80213830

OSContext* OSGetCurrentContext(void) {
    return __OSCurrentContext;
} // 0x80213890

/*
 * OSSaveContext
 *
 * Saves the caller's registers into context; returns 0, or 1 when the
 * context is resumed by OSLoadContext.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm u32 OSSaveContext(register OSContext* context) {
    nofralloc
    stmw r13, context->gpr[13]
    mfspr r0, GQR1
    stw r0, context->gqr[1]
    mfspr r0, GQR2
    stw r0, context->gqr[2]
    mfspr r0, GQR3
    stw r0, context->gqr[3]
    mfspr r0, GQR4
    stw r0, context->gqr[4]
    mfspr r0, GQR5
    stw r0, context->gqr[5]
    mfspr r0, GQR6
    stw r0, context->gqr[6]
    mfspr r0, GQR7
    stw r0, context->gqr[7]
    mfcr r0
    stw r0, context->cr
    mflr r0
    stw r0, context->lr
    stw r0, context->srr0
    mfmsr r0
    stw r0, context->srr1
    mfctr r0
    stw r0, context->ctr
    mfxer r0
    stw r0, context->xer
    stw r1, context->gpr[1]
    stw r2, context->gpr[2]
    li r0, 0x1
    stw r0, context->gpr[3]
    li r3, 0
    blr
} // 0x802138A0

/*
 * OSLoadContext
 *
 * Resumes context with rfi. A context interrupted inside the window of
 * OSDisableInterrupts is restarted at its beginning.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm void OSLoadContext(register OSContext* context) {
    nofralloc
    lis r4, __RAS_OSDisableInterrupts_begin@ha
    lwz r6, context->srr0
    addi r5, r4, __RAS_OSDisableInterrupts_begin@l
    cmplw r6, r5
    ble _notInRAS
    lis r4, __RAS_OSDisableInterrupts_end@ha
    addi r0, r4, __RAS_OSDisableInterrupts_end@l
    cmplw r6, r0
    bge _notInRAS
    stw r5, context->srr0

_notInRAS:
    lwz r0, context->gpr[0]
    lwz r1, context->gpr[1]
    lwz r2, context->gpr[2]

    lhz r4, context->state
    rlwinm. r5, r4, 0, 30, 30   // OS_CONTEXT_STATE_EXC
    beq notexc
    rlwinm r4, r4, 0, 31, 29
    sth r4, context->state
    lmw r5, context->gpr[5]
    b misc

notexc:
    lmw r13, context->gpr[13]

misc:
    lwz r4, context->gqr[1]
    mtspr GQR1, r4
    lwz r4, context->gqr[2]
    mtspr GQR2, r4
    lwz r4, context->gqr[3]
    mtspr GQR3, r4
    lwz r4, context->gqr[4]
    mtspr GQR4, r4
    lwz r4, context->gqr[5]
    mtspr GQR5, r4
    lwz r4, context->gqr[6]
    mtspr GQR6, r4
    lwz r4, context->gqr[7]
    mtspr GQR7, r4

    lwz r4, context->cr
    mtcrf 0xFF, r4
    lwz r4, context->lr
    mtlr r4
    lwz r4, context->ctr
    mtctr r4
    lwz r4, context->xer
    mtxer r4

    mfmsr r4
    rlwinm r4, r4, 0, 17, 15    // clear MSR[EE]
    rlwinm r4, r4, 0, 31, 29    // clear MSR[RI]
    mtmsr r4

    lwz r4, context->srr0
    mtsrr0 r4
    lwz r4, context->srr1
    mtsrr1 r4

    lwz r4, context->gpr[4]
    lwz r3, context->gpr[3]
    rfi
} // 0x80213920

// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm u32 OSGetStackPointer(void) {
    nofralloc
    mr r3, r1
    blr
} // 0x80213A00

/*
 * OSSwitchFiber
 *
 * Calls pc on the stack newsp and switches back when it returns.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm void OSSwitchFiber(register u32 pc, register u32 newsp) {
    nofralloc
    mflr r0
    mr r5, r1
    stwu r5, -8(newsp)
    mr r1, newsp
    stw r0, 4(r5)
    mtlr pc
    blrl
    lwz r5, 0(r1)
    lwz r0, 4(r5)
    mtlr r0
    mr r1, r5
    blr
} // 0x80213A10

/*
 * OSSwitchFiberEx
 *
 * OSSwitchFiber that passes four arguments to pc.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm void OSSwitchFiberEx(register u32 arg0, register u32 arg1, register u32 arg2, register u32 arg3,
                         register u32 pc, register u32 newsp) {
    nofralloc
    mflr r0
    mr r9, r1
    stwu r9, -8(newsp)
    mr r1, newsp
    stw r0, 4(r9)
    mtlr pc
    blrl
    lwz r5, 0(r1)
    lwz r0, 4(r5)
    mtlr r0
    mr r1, r5
    blr
} // 0x80213A40

/*
 * OSClearContext
 *
 * Marks context as holding no saved state, releasing the FPU if the
 * context owns it.
 */
void OSClearContext(register OSContext* context) {
    context->mode = 0;
    context->state = 0;
    if (context == __OSFPUContext) {
        __OSFPUContext = NULL;
    }
} // 0x80213A70

/*
 * OSInitContext
 *
 * Sets up context to start at pc on the stack newsp with the SDA bases
 * of the caller.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
asm void OSInitContext(register OSContext* context, register u32 pc, register u32 newsp) {
    nofralloc
    stw pc, OS_CONTEXT_SRR0(context)
    stw newsp, OS_CONTEXT_R1(context)
    li r11, 0
    ori r11, r11, 0x9032    // MSR[EE, FP, ME, IR, DR, RI]
    stw r11, OS_CONTEXT_SRR1(context)
    li r0, 0
    stw r0, OS_CONTEXT_CR(context)
    stw r0, OS_CONTEXT_XER(context)

    stw r2, OS_CONTEXT_R2(context)
    stw r13, OS_CONTEXT_R13(context)

    stw r0, OS_CONTEXT_R3(context)
    stw r0, OS_CONTEXT_R4(context)
    stw r0, OS_CONTEXT_R5(context)
    stw r0, OS_CONTEXT_R6(context)
    stw r0, OS_CONTEXT_R7(context)
    stw r0, OS_CONTEXT_R8(context)
    stw r0, OS_CONTEXT_R9(context)
    stw r0, OS_CONTEXT_R10(context)
    stw r0, OS_CONTEXT_R11(context)
    stw r0, OS_CONTEXT_R12(context)

    stw r0, OS_CONTEXT_R14(context)
    stw r0, OS_CONTEXT_R15(context)
    stw r0, OS_CONTEXT_R16(context)
    stw r0, OS_CONTEXT_R17(context)
    stw r0, OS_CONTEXT_R18(context)
    stw r0, OS_CONTEXT_R19(context)
    stw r0, OS_CONTEXT_R20(context)
    stw r0, OS_CONTEXT_R21(context)
    stw r0, OS_CONTEXT_R22(context)
    stw r0, OS_CONTEXT_R23(context)
    stw r0, OS_CONTEXT_R24(context)
    stw r0, OS_CONTEXT_R25(context)
    stw r0, OS_CONTEXT_R26(context)
    stw r0, OS_CONTEXT_R27(context)
    stw r0, OS_CONTEXT_R28(context)
    stw r0, OS_CONTEXT_R29(context)
    stw r0, OS_CONTEXT_R30(context)
    stw r0, OS_CONTEXT_R31(context)

    stw r0, OS_CONTEXT_GQR0(context)
    stw r0, OS_CONTEXT_GQR1(context)
    stw r0, OS_CONTEXT_GQR2(context)
    stw r0, OS_CONTEXT_GQR3(context)
    stw r0, OS_CONTEXT_GQR4(context)
    stw r0, OS_CONTEXT_GQR5(context)
    stw r0, OS_CONTEXT_GQR6(context)
    stw r0, OS_CONTEXT_GQR7(context)

    b OSClearContext
} // 0x80213AA0

/*
 * OSDumpContext
 *
 * Prints context: GPRs, special registers, GQRs, the FPRs and PS1 halves
 * if saved, and up to 16 frames of the back chain.
 */
void OSDumpContext(OSContext* context) {
    u32 i;
    u32* p;

    OSReport("------------------------- Context 0x%08x -------------------------\n", context);

    for (i = 0; i < 16; ++i) {
        OSReport("r%-2d  = 0x%08x (%14d)  r%-2d  = 0x%08x (%14d)\n", i, context->gpr[i], context->gpr[i], i + 16,
                 context->gpr[i + 16], context->gpr[i + 16]);
    }

    OSReport("LR   = 0x%08x                   CR   = 0x%08x\n", context->lr, context->cr);
    OSReport("SRR0 = 0x%08x                   SRR1 = 0x%08x\n", context->srr0, context->srr1);

    OSReport("\nGQRs----------\n");
    for (i = 0; i < 4; ++i) {
        OSReport("gqr%d = 0x%08x \t gqr%d = 0x%08x\n", i, context->gqr[i], i + 4, context->gqr[i + 4]);
    }

    if (context->state & OS_CONTEXT_STATE_FPSAVED) {
        OSContext* currentContext;
        OSContext fpuContext;
        BOOL enabled;

        /* The FPRs are printed through a scratch context, so OSReport's own FPU use cannot clobber them. */
        enabled = OSDisableInterrupts();
        currentContext = OSGetCurrentContext();
        OSClearContext(&fpuContext);
        OSSetCurrentContext(&fpuContext);

        OSReport("\n\nFPRs----------\n");
        for (i = 0; i < 32; i += 2) {
            OSReport("fr%d \t= %d \t fr%d \t= %d\n", i, (u32)context->fpr[i], i + 1, (u32)context->fpr[i + 1]);
        }

        OSReport("\n\nPSFs----------\n");
        for (i = 0; i < 32; i += 2) {
            OSReport("ps%d \t= 0x%x \t ps%d \t= 0x%x\n", i, (u32)context->psf[i], i + 1, (u32)context->psf[i + 1]);
        }

        OSClearContext(&fpuContext);
        OSSetCurrentContext(currentContext);
        OSRestoreInterrupts(enabled);
    }

    OSReport("\nAddress:      Back Chain    LR Save\n");
    for (i = 0, p = (u32*)context->gpr[1]; p && (u32)p != 0xFFFFFFFF && i++ < 16; p = (u32*)*p) {
        OSReport("0x%08x:   0x%08x    0x%08x\n", p, p[0], p[1]);
    }
} // 0x80213B60

/*
 * OSSwitchFPUContext
 *
 * FPU-unavailable exception handler: moves the FPU state from its owner
 * to the current context, enables the FPU for it and resumes it.
 */
// ORIGINAL-ASM: register save/restore and rfi; an asm function in the public RVL SDK OSContext.c
static asm void OSSwitchFPUContext(register __OSException exception, register OSContext* context) {
    nofralloc
    mfmsr r5
    ori r5, r5, 0x2000      // MSR[FP]
    mtmsr r5
    isync
    lwz r5, context->srr1
    ori r5, r5, 0x2000      // MSR[FP]
    mtsrr1 r5
    lis r3, OS_CURRENT_FPU_CONTEXT_ADDR@ha
    lwz r5, OS_CURRENT_FPU_CONTEXT_ADDR@l(r3)
    stw context, OS_CURRENT_FPU_CONTEXT_ADDR@l(r3)
    cmpw r5, context
    beq _restoreAndExit
    cmpwi r5, 0
    beq _loadNewFPUContext
    bl __OSSaveFPUContext

_loadNewFPUContext:
    bl __OSLoadFPUContext

_restoreAndExit:
    lwz r3, context->cr
    mtcrf 0xFF, r3
    lwz r3, context->lr
    mtlr r3
    lwz r3, context->srr0
    mtsrr0 r3
    lwz r3, context->ctr
    mtctr r3
    lwz r3, context->xer
    mtxer r3
    lhz r3, context->state
    rlwinm r3, r3, 0, 31, 29    // clear OS_CONTEXT_STATE_EXC
    sth r3, context->state
    lwz r5, context->gpr[5]
    lwz r3, context->gpr[3]
    lwz r4, context->gpr[4]
    rfi
} // 0x80213DC0

/*
 * __OSContextInit
 */
void __OSContextInit(void) {
    __OSSetExceptionHandler(__OS_EXCEPTION_FLOATING_POINT, OSSwitchFPUContext);
    __OSFPUContext = NULL;
    DBPrintf("FPU-unavailable handler installed\n");
} // 0x80213E50
