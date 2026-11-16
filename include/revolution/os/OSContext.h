#ifndef REVOLUTION_OS_OSCONTEXT_H
#define REVOLUTION_OS_OSCONTEXT_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Byte offsets into OSContext, for assembly. */
#define OS_CONTEXT_R0     0x0
#define OS_CONTEXT_R1     0x4
#define OS_CONTEXT_R2     0x8
#define OS_CONTEXT_R3     0xC
#define OS_CONTEXT_R4     0x10
#define OS_CONTEXT_R5     0x14
#define OS_CONTEXT_R6     0x18
#define OS_CONTEXT_R7     0x1C
#define OS_CONTEXT_R8     0x20
#define OS_CONTEXT_R9     0x24
#define OS_CONTEXT_R10    0x28
#define OS_CONTEXT_R11    0x2C
#define OS_CONTEXT_R12    0x30
#define OS_CONTEXT_R13    0x34
#define OS_CONTEXT_R14    0x38
#define OS_CONTEXT_R15    0x3C
#define OS_CONTEXT_R16    0x40
#define OS_CONTEXT_R17    0x44
#define OS_CONTEXT_R18    0x48
#define OS_CONTEXT_R19    0x4C
#define OS_CONTEXT_R20    0x50
#define OS_CONTEXT_R21    0x54
#define OS_CONTEXT_R22    0x58
#define OS_CONTEXT_R23    0x5C
#define OS_CONTEXT_R24    0x60
#define OS_CONTEXT_R25    0x64
#define OS_CONTEXT_R26    0x68
#define OS_CONTEXT_R27    0x6C
#define OS_CONTEXT_R28    0x70
#define OS_CONTEXT_R29    0x74
#define OS_CONTEXT_R30    0x78
#define OS_CONTEXT_R31    0x7C
#define OS_CONTEXT_CR        0x80
#define OS_CONTEXT_LR        0x84
#define OS_CONTEXT_CTR       0x88
#define OS_CONTEXT_XER       0x8C
#define OS_CONTEXT_FPSCR_PAD 0x190
#define OS_CONTEXT_SRR0      0x198
#define OS_CONTEXT_SRR1      0x19C
#define OS_CONTEXT_MODE      0x1A0
#define OS_CONTEXT_STATE     0x1A2
#define OS_CONTEXT_GQR0      0x1A4
#define OS_CONTEXT_GQR1      0x1A8
#define OS_CONTEXT_GQR2      0x1AC
#define OS_CONTEXT_GQR3      0x1B0
#define OS_CONTEXT_GQR4      0x1B4
#define OS_CONTEXT_GQR5      0x1B8
#define OS_CONTEXT_GQR6      0x1BC
#define OS_CONTEXT_GQR7      0x1C0
#define OS_CONTEXT_PSF0      0x1C8
#define OS_CONTEXT_PSF1      0x1D0
#define OS_CONTEXT_PSF2      0x1D8
#define OS_CONTEXT_PSF3      0x1E0
#define OS_CONTEXT_PSF4      0x1E8
#define OS_CONTEXT_PSF5      0x1F0
#define OS_CONTEXT_PSF6      0x1F8
#define OS_CONTEXT_PSF7      0x200
#define OS_CONTEXT_PSF8      0x208
#define OS_CONTEXT_PSF9      0x210
#define OS_CONTEXT_PSF10     0x218
#define OS_CONTEXT_PSF11     0x220
#define OS_CONTEXT_PSF12     0x228
#define OS_CONTEXT_PSF13     0x230
#define OS_CONTEXT_PSF14     0x238
#define OS_CONTEXT_PSF15     0x240
#define OS_CONTEXT_PSF16     0x248
#define OS_CONTEXT_PSF17     0x250
#define OS_CONTEXT_PSF18     0x258
#define OS_CONTEXT_PSF19     0x260
#define OS_CONTEXT_PSF20     0x268
#define OS_CONTEXT_PSF21     0x270
#define OS_CONTEXT_PSF22     0x278
#define OS_CONTEXT_PSF23     0x280
#define OS_CONTEXT_PSF24     0x288
#define OS_CONTEXT_PSF25     0x290
#define OS_CONTEXT_PSF26     0x298
#define OS_CONTEXT_PSF27     0x2A0
#define OS_CONTEXT_PSF28     0x2A8
#define OS_CONTEXT_PSF29     0x2B0
#define OS_CONTEXT_PSF30     0x2B8
#define OS_CONTEXT_PSF31     0x2C0

/* OSContext.state */
#define OS_CONTEXT_STATE_FPSAVED 0x01 /* the FPRs are saved in the context */
#define OS_CONTEXT_STATE_EXC     0x02 /* saved by an exception */

/* Low-memory pointers to the running context and to the context that owns the FPU. */
#define OS_CURRENT_CONTEXT_PHYS_ADDR 0x800000C0
#define OS_CURRENT_CONTEXT_ADDR      0x800000D4
#define OS_CURRENT_FPU_CONTEXT_ADDR  0x800000D8

typedef struct OSContext {
    u32 gpr[32];
    u32 cr;
    u32 lr;
    u32 ctr;
    u32 xer;
    f64 fpr[32];
    u32 fpscr_pad;
    u32 fpscr;
    u32 srr0;
    u32 srr1;
    u16 mode;
    u16 state;
    u32 gqr[8];
    u32 psf_pad;
    f64 psf[32];
} OSContext;

struct OSContext* __OSCurrentContext : OS_CURRENT_CONTEXT_ADDR;
struct OSContext* __OSFPUContext : OS_CURRENT_FPU_CONTEXT_ADDR;

void OSSaveFPUContext(OSContext* fpuContext);
void OSSetCurrentContext(OSContext* context);
OSContext* OSGetCurrentContext(void);
u32 OSSaveContext(OSContext* context);
void OSLoadContext(OSContext* context);
void OSClearContext(OSContext* context);
void OSDumpContext(OSContext* context);
u32 OSGetStackPointer(void);
void OSSwitchFiber(u32 pc, u32 newsp);
void OSSwitchFiberEx(u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 pc, u32 newsp);
void OSInitContext(OSContext* context, u32 pc, u32 newsp);
void __OSContextInit(void);

#ifdef __cplusplus
}
#endif

#endif
