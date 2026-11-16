#ifndef REVOLUTION_DSP_H
#define REVOLUTION_DSP_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DSP interface registers: indices into the halfword block __DSPRegs
 * (revolution/hardware.h). The names are invented; the SDK uses bare numbers.
 */
#define DSP_REG_MAIL_TO_H        0  /* mailbox to the DSP, high half (bit 15: mail not yet taken) */
#define DSP_REG_MAIL_TO_L        1  /* mailbox to the DSP, low half */
#define DSP_REG_MAIL_FROM_H      2  /* mailbox from the DSP, high half (bit 15: mail waiting) */
#define DSP_REG_MAIL_FROM_L      3  /* mailbox from the DSP, low half */
#define DSP_REG_CSR              5  /* control/status */
#define DSP_REG_AI_DMA_START_H   24 /* audio DMA start address, high half */
#define DSP_REG_AI_DMA_START_L   25 /* audio DMA start address, low half */
#define DSP_REG_AI_DMA_CTRL      27 /* audio DMA enable and length in 32-byte blocks */
#define DSP_REG_AI_DMA_LEFT      29 /* audio DMA blocks left */

/* DSP_REG_CSR bits. */
#define DSP_CSR_AI_DMA_INT       0x08 /* audio DMA interrupt flag (write 1 to clear) */
#define DSP_CSR_ARAM_INT         0x20 /* ARAM DMA interrupt flag (write 1 to clear) */
#define DSP_CSR_DSP_INT          0x80 /* DSP interrupt flag (write 1 to clear) */

/* DSP_REG_AI_DMA_CTRL fields. */
#define DSP_AI_DMA_ENABLE        0x8000
#define DSP_AI_DMA_LENGTH_MASK   0x7FFF


/* DSP task: one program the DSP driver loads into the DSP and runs. */
typedef struct STRUCT_DSP_TASK DSPTask;
typedef void (*DSPCallback)(void* task);

/* DSPTask flags (zeldaret/tp dsp.h). */
#define DSP_TASK_FLAG_CLEARALL 0x00000000
#define DSP_TASK_FLAG_ATTACHED 0x00000001
#define DSP_TASK_FLAG_CANCEL   0x00000002

struct STRUCT_DSP_TASK {
    /* 0x00 */ volatile u32 state; // volatile: SDK declaration (zeldaret/tp dsp.h); the DSP interrupt handler updates it
    /* 0x04 */ volatile u32 priority; // volatile: SDK declaration (zeldaret/tp dsp.h)
    /* 0x08 */ volatile u32 flags; // volatile: SDK declaration (zeldaret/tp dsp.h)
    /* 0x0C */ void* iram_mmem_addr; /* instruction memory image in main memory */
    /* 0x10 */ u32 iram_length;
    /* 0x14 */ u32 iram_addr;        /* its address in DSP instruction memory */
    /* 0x18 */ void* dram_mmem_addr; /* data memory image in main memory */
    /* 0x1C */ u32 dram_length;
    /* 0x20 */ u32 dram_addr;        /* its address in DSP data memory */
    /* 0x24 */ u16 dsp_init_vector;
    /* 0x26 */ u16 dsp_resume_vector;
    /* 0x28 */ DSPCallback init_cb;
    /* 0x2C */ DSPCallback res_cb;
    /* 0x30 */ DSPCallback done_cb;
    /* 0x34 */ DSPCallback req_cb;
    /* 0x38 */ DSPTask* next;
    /* 0x3C */ DSPTask* prev;
    /* 0x40 */ s64 t_context;
    /* 0x48 */ s64 t_task;
};

/* Sends a mail to the DSP and waits until the DSP has taken it (ogws dsp_task.h). */
#define DSP_SEND_MAIL_SYNC(mail)               \
    do {                                       \
        DSPSendMailToDSP((u32)(mail));         \
        while (DSPCheckMailToDSP()) {          \
            ;                                  \
        }                                      \
    } while (0)

BOOL DSPCheckMailToDSP(void);
BOOL DSPCheckMailFromDSP(void);
u32 DSPReadMailFromDSP(void);
void DSPSendMailToDSP(u32 mail);
void DSPInit(void);
BOOL DSPCheckInit(void);
DSPTask* DSPAddTask(DSPTask* task);
DSPTask* DSPCancelTask(DSPTask* task);
DSPTask* DSPAssertTask(DSPTask* task);

#ifdef __cplusplus
}
#endif

#endif
