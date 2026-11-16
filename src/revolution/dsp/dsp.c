// Reference: zeldaret/tp libs/revolution/src/dsp/dsp.c
#include <revolution/dsp.h>
#include <revolution/dsp/__dsp.h>
#include <revolution/hardware.h>
#include <revolution/os.h>

#define BUILD_DATE "Feb 27 2009"
#define BUILD_TIME "10:01:57"

const char* __DSPVersion = "<< RVL_SDK - DSP \trelease build: " BUILD_DATE " " BUILD_TIME " (0x4302_145) >>";

static BOOL __DSP_init_flag;

/*
 * DSPCheckMailToDSP
 *
 * Whether the DSP has yet to take the mail written to its mailbox.
 */
BOOL DSPCheckMailToDSP(void) {
    return (__DSPRegs[DSP_REG_MAIL_TO_H] & (1 << 15)) >> 15;
} // 0x80236C10

/*
 * DSPCheckMailFromDSP
 */
BOOL DSPCheckMailFromDSP(void) {
    return (__DSPRegs[DSP_REG_MAIL_FROM_H] & (1 << 15)) >> 15;
} // 0x80236C20

/*
 * DSPReadMailFromDSP
 */
u32 DSPReadMailFromDSP(void) {
    return (__DSPRegs[DSP_REG_MAIL_FROM_H] << 16) | __DSPRegs[DSP_REG_MAIL_FROM_L];
} // 0x80236C30

/*
 * DSPSendMailToDSP
 */
void DSPSendMailToDSP(u32 mail) {
    __DSPRegs[DSP_REG_MAIL_TO_H] = mail >> 16;
    __DSPRegs[DSP_REG_MAIL_TO_L] = mail & 0xFFFF;
} // 0x80236C50

/*
 * DSPInit
 */
void DSPInit(void) {
    BOOL old;
    u16 tmp;

    __DSP_debug_printf("DSPInit(): Build Date: %s %s\n", BUILD_DATE, BUILD_TIME);

    if (__DSP_init_flag == 1) {
        return;
    }

    OSRegisterVersion(__DSPVersion);

    old = OSDisableInterrupts();
    __OSSetInterruptHandler(__OS_INTERRUPT_DSP_DSP, __DSPHandler);
    __OSUnmaskInterrupts(OS_INTERRUPTMASK_DSP_DSP);

    tmp = __DSPRegs[DSP_REG_CSR];
    tmp = (tmp & ~0xA8) | 0x800;
    __DSPRegs[DSP_REG_CSR] = tmp;

    tmp = __DSPRegs[DSP_REG_CSR];
    tmp = tmp & ~0xAC;
    __DSPRegs[DSP_REG_CSR] = tmp;

    __DSP_first_task = __DSP_last_task = __DSP_curr_task = __DSP_tmp_task = NULL;
    __DSP_init_flag = 1;

    OSRestoreInterrupts(old);
} // 0x80236C70

/*
 * DSPCheckInit
 */
BOOL DSPCheckInit(void) {
    return __DSP_init_flag;
} // 0x80236D30

/*
 * DSPAddTask
 */
DSPTask* DSPAddTask(DSPTask* task) {
    BOOL old;

    old = OSDisableInterrupts();

    __DSP_insert_task(task);
    task->state = 0;
    task->flags = 1;

    OSRestoreInterrupts(old);
    if (task == __DSP_first_task) {
        __DSP_boot_task(task);
    }
    return task;
} // 0x80236D40

/*
 * DSPCancelTask
 */
DSPTask* DSPCancelTask(DSPTask* task) {
    BOOL old;

    old = OSDisableInterrupts();

    task->flags |= 2;

    OSRestoreInterrupts(old);
    return task;
} // 0x80236DB0

/*
 * DSPAssertInt
 *
 * Raises the DSP interrupt (the hardware interrupt request bit).
 */
void DSPAssertInt(void) {
    BOOL old;
    u16 tmp;

    old = OSDisableInterrupts();
    tmp = __DSPRegs[DSP_REG_CSR];
    tmp = (tmp & ~0xA8) | 2;
    __DSPRegs[DSP_REG_CSR] = tmp;
    OSRestoreInterrupts(old);
}

/*
 * DSPAssertTask
 */
DSPTask* DSPAssertTask(DSPTask* task) {
    s32 old;

    old = OSDisableInterrupts();

    if (__DSP_curr_task == task) {
        __DSP_rude_task = task;
        __DSP_rude_task_pending = 1;
        OSRestoreInterrupts(old);
        return task;
    }

    if (task->priority < __DSP_curr_task->priority) {
        __DSP_rude_task = task;
        __DSP_rude_task_pending = 1;
        if (__DSP_curr_task->state == 1) {
            DSPAssertInt();
        }
        OSRestoreInterrupts(old);
        return task;
    }

    OSRestoreInterrupts(old);
    return NULL;
} // 0x80236DF0
