#ifndef REVOLUTION_OS_OSINTERRUPT_H
#define REVOLUTION_OS_OSINTERRUPT_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSTime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef s16 __OSInterrupt;
typedef u32 OSInterruptMask;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);

/* Interrupt sources, numbered from the most significant bit of an OSInterruptMask. */
#define __OS_INTERRUPT_MEM_0          0
#define __OS_INTERRUPT_MEM_1          1
#define __OS_INTERRUPT_MEM_2          2
#define __OS_INTERRUPT_MEM_3          3
#define __OS_INTERRUPT_MEM_ADDRESS    4
#define __OS_INTERRUPT_DSP_AI         5
#define __OS_INTERRUPT_DSP_ARAM       6
#define __OS_INTERRUPT_DSP_DSP        7
#define __OS_INTERRUPT_AI_AI          8
#define __OS_INTERRUPT_EXI_0_EXI      9
#define __OS_INTERRUPT_EXI_0_TC       10
#define __OS_INTERRUPT_EXI_0_EXT      11
#define __OS_INTERRUPT_EXI_1_EXI      12
#define __OS_INTERRUPT_EXI_1_TC       13
#define __OS_INTERRUPT_EXI_1_EXT      14
#define __OS_INTERRUPT_EXI_2_EXI      15
#define __OS_INTERRUPT_EXI_2_TC       16
#define __OS_INTERRUPT_PI_CP          17
#define __OS_INTERRUPT_PI_PE_TOKEN    18
#define __OS_INTERRUPT_PI_PE_FINISH   19
#define __OS_INTERRUPT_PI_SI          20
#define __OS_INTERRUPT_PI_DI          21
#define __OS_INTERRUPT_PI_RSW         22
#define __OS_INTERRUPT_PI_ERROR       23
#define __OS_INTERRUPT_PI_VI          24
#define __OS_INTERRUPT_PI_DEBUG       25
#define __OS_INTERRUPT_PI_HSP         26
#define __OS_INTERRUPT_PI_ACR         27
#define __OS_INTERRUPT_MAX            32

#define OSInterruptMaskOf(interrupt) (0x80000000u >> (interrupt))

#define OS_INTERRUPTMASK_MEM_0          OSInterruptMaskOf(__OS_INTERRUPT_MEM_0)
#define OS_INTERRUPTMASK_MEM_1          OSInterruptMaskOf(__OS_INTERRUPT_MEM_1)
#define OS_INTERRUPTMASK_MEM_2          OSInterruptMaskOf(__OS_INTERRUPT_MEM_2)
#define OS_INTERRUPTMASK_MEM_3          OSInterruptMaskOf(__OS_INTERRUPT_MEM_3)
#define OS_INTERRUPTMASK_MEM_ADDRESS    OSInterruptMaskOf(__OS_INTERRUPT_MEM_ADDRESS)
#define OS_INTERRUPTMASK_MEM                                                                       \
    (OS_INTERRUPTMASK_MEM_0 | OS_INTERRUPTMASK_MEM_1 | OS_INTERRUPTMASK_MEM_2 |                    \
     OS_INTERRUPTMASK_MEM_3 | OS_INTERRUPTMASK_MEM_ADDRESS)
#define OS_INTERRUPTMASK_DSP_AI         OSInterruptMaskOf(__OS_INTERRUPT_DSP_AI)
#define OS_INTERRUPTMASK_DSP_ARAM       OSInterruptMaskOf(__OS_INTERRUPT_DSP_ARAM)
#define OS_INTERRUPTMASK_DSP_DSP        OSInterruptMaskOf(__OS_INTERRUPT_DSP_DSP)
#define OS_INTERRUPTMASK_DSP            (OS_INTERRUPTMASK_DSP_AI | OS_INTERRUPTMASK_DSP_ARAM | OS_INTERRUPTMASK_DSP_DSP)
#define OS_INTERRUPTMASK_AI_AI          OSInterruptMaskOf(__OS_INTERRUPT_AI_AI)
#define OS_INTERRUPTMASK_AI             OS_INTERRUPTMASK_AI_AI
#define OS_INTERRUPTMASK_EXI_0_EXI      OSInterruptMaskOf(__OS_INTERRUPT_EXI_0_EXI)
#define OS_INTERRUPTMASK_EXI_0_TC       OSInterruptMaskOf(__OS_INTERRUPT_EXI_0_TC)
#define OS_INTERRUPTMASK_EXI_0_EXT      OSInterruptMaskOf(__OS_INTERRUPT_EXI_0_EXT)
#define OS_INTERRUPTMASK_EXI_0          (OS_INTERRUPTMASK_EXI_0_EXI | OS_INTERRUPTMASK_EXI_0_TC | OS_INTERRUPTMASK_EXI_0_EXT)
#define OS_INTERRUPTMASK_EXI_1_EXI      OSInterruptMaskOf(__OS_INTERRUPT_EXI_1_EXI)
#define OS_INTERRUPTMASK_EXI_1_TC       OSInterruptMaskOf(__OS_INTERRUPT_EXI_1_TC)
#define OS_INTERRUPTMASK_EXI_1_EXT      OSInterruptMaskOf(__OS_INTERRUPT_EXI_1_EXT)
#define OS_INTERRUPTMASK_EXI_1          (OS_INTERRUPTMASK_EXI_1_EXI | OS_INTERRUPTMASK_EXI_1_TC | OS_INTERRUPTMASK_EXI_1_EXT)
#define OS_INTERRUPTMASK_EXI_2_EXI      OSInterruptMaskOf(__OS_INTERRUPT_EXI_2_EXI)
#define OS_INTERRUPTMASK_EXI_2_TC       OSInterruptMaskOf(__OS_INTERRUPT_EXI_2_TC)
#define OS_INTERRUPTMASK_EXI_2          (OS_INTERRUPTMASK_EXI_2_EXI | OS_INTERRUPTMASK_EXI_2_TC)
#define OS_INTERRUPTMASK_EXI                                                                       \
    (OS_INTERRUPTMASK_EXI_0 | OS_INTERRUPTMASK_EXI_1 | OS_INTERRUPTMASK_EXI_2)
#define OS_INTERRUPTMASK_PI_CP          OSInterruptMaskOf(__OS_INTERRUPT_PI_CP)
#define OS_INTERRUPTMASK_PI_PE_TOKEN    OSInterruptMaskOf(__OS_INTERRUPT_PI_PE_TOKEN)
#define OS_INTERRUPTMASK_PI_PE_FINISH   OSInterruptMaskOf(__OS_INTERRUPT_PI_PE_FINISH)
#define OS_INTERRUPTMASK_PI_SI          OSInterruptMaskOf(__OS_INTERRUPT_PI_SI)
#define OS_INTERRUPTMASK_PI_DI          OSInterruptMaskOf(__OS_INTERRUPT_PI_DI)
#define OS_INTERRUPTMASK_PI_RSW         OSInterruptMaskOf(__OS_INTERRUPT_PI_RSW)
#define OS_INTERRUPTMASK_PI_ERROR       OSInterruptMaskOf(__OS_INTERRUPT_PI_ERROR)
#define OS_INTERRUPTMASK_PI_VI          OSInterruptMaskOf(__OS_INTERRUPT_PI_VI)
#define OS_INTERRUPTMASK_PI_DEBUG       OSInterruptMaskOf(__OS_INTERRUPT_PI_DEBUG)
#define OS_INTERRUPTMASK_PI_HSP         OSInterruptMaskOf(__OS_INTERRUPT_PI_HSP)
#define OS_INTERRUPTMASK_PI_ACR         OSInterruptMaskOf(__OS_INTERRUPT_PI_ACR)
#define OS_INTERRUPTMASK_PI                                                                        \
    (OS_INTERRUPTMASK_PI_CP | OS_INTERRUPTMASK_PI_PE_TOKEN | OS_INTERRUPTMASK_PI_PE_FINISH |       \
     OS_INTERRUPTMASK_PI_SI | OS_INTERRUPTMASK_PI_DI | OS_INTERRUPTMASK_PI_RSW |                   \
     OS_INTERRUPTMASK_PI_ERROR | OS_INTERRUPTMASK_PI_VI | OS_INTERRUPTMASK_PI_DEBUG |              \
     OS_INTERRUPTMASK_PI_HSP | OS_INTERRUPTMASK_PI_ACR)

/* The last external interrupt taken, for the crash report. */
extern volatile __OSInterrupt __OSLastInterrupt; // volatile: SDK OSInterrupt.h; written by the interrupt handler
extern volatile u32 __OSLastInterruptSrr0;       // volatile: SDK OSInterrupt.h; written by the interrupt handler
extern volatile OSTime __OSLastInterruptTime;    // volatile: SDK OSInterrupt.h; written by the interrupt handler

/* Bounds of the window in OSDisableInterrupts that OSLoadContext restarts from the beginning (labels in its asm body). */
void __RAS_OSDisableInterrupts_begin(void);
void __RAS_OSDisableInterrupts_end(void);

BOOL OSDisableInterrupts(void);
BOOL OSEnableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt);
void __OSInterruptInit(void);
OSInterruptMask __OSMaskInterrupts(OSInterruptMask mask);
OSInterruptMask __OSUnmaskInterrupts(OSInterruptMask mask);

#ifdef __cplusplus
}
#endif

#endif
