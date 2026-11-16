#ifndef REVOLUTION_BASE_PPCARCH_H
#define REVOLUTION_BASE_PPCARCH_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Broadway SPR numbers for asm. The assembler encodes its own HID2 name as
 * SPR 1011 (which is HID4 on Broadway) and has no HID4 name.
 */
#define SPR_HID2 920
#define SPR_HID4 1011

/* MSR: machine state register. */
#define MSR_FP  0x00002000 /* floating point available */
#define MSR_FE0 0x00000800 /* floating-point exception mode 0 */
#define MSR_FE1 0x00000100 /* floating-point exception mode 1 */
#define MSR_IR  0x00000020 /* instruction address translation */
#define MSR_DR  0x00000010 /* data address translation */
#define MSR_RI  0x00000002 /* recoverable exception */

/* FPSCR: floating-point status and control register. */
#define FPSCR_ENABLE 0x000000F8 /* VE, OE, UE, ZE, XE: exception enables */
#define FPSCR_NI     0x00000004 /* non-IEEE mode */

/*
 * The sticky exception bits: FX, OX, UX, ZX, XX and the VX causes
 * (VXSNAN, VXISI, VXIDI, VXZDZ, VXIMZ, VXVC, VXSOFT, VXSQRT, VXCVI).
 */
#define FPSCR_STICKY_EXCEPTIONS 0x9FFA0700

/* HID0: hardware implementation register 0. */
#define HID0_ICE 0x00008000 /* instruction cache enable */
#define HID0_DCE 0x00004000 /* data cache enable */
#define HID0_SPD 0x00000200 /* speculative cache access disable */

/* HID2: locked cache enable and the locked-cache DMA error bits. */
#define HID2_LCE    0x10000000 /* locked cache enable */
#define HID2_DCHERR 0x00800000 /* locked cache tag already in the cache */
#define HID2_DNCERR 0x00400000 /* DMA accessed normal cache */
#define HID2_DCMERR 0x00200000 /* DMA missed in the data cache */
#define HID2_DQOERR 0x00100000 /* DMA queue overflow */

/* L2CR: L2 cache control register. */
#define L2CR_L2E  0x80000000 /* L2 enable */
#define L2CR_L2I  0x00200000 /* global invalidate */
#define L2CR_L2IP 0x00000001 /* invalidate in progress */

/* SRR1 bit set by a machine check raised by locked-cache DMA. */
#define SRR1_DMA_BIT 0x00200000

/* SRR1 bit set by a program exception raised by an enabled FP exception. */
#define SRR1_PROGRAM_FPE 0x00100000

/* HID4: Broadway hardware implementation register 4. */
#define HID4_H4A 0x80000000 /* must stay set (Broadway errata) */

u32 PPCMfmsr(void);
void PPCMtmsr(u32 newMSR);
u32 PPCMfhid0(void);
void PPCMthid0(u32 newHID0);
u32 PPCMfl2cr(void);
void PPCMtl2cr(u32 newL2cr);
void PPCMtdec(u32 newDec);
void PPCSync(void);
void PPCHalt(void);
void PPCMtmmcr0(u32 newMmcr0);
void PPCMtmmcr1(u32 newMmcr1);
void PPCMtpmc1(u32 newPmc1);
void PPCMtpmc2(u32 newPmc2);
void PPCMtpmc3(u32 newPmc3);
void PPCMtpmc4(u32 newPmc4);
u32 PPCMffpscr(void);
void PPCMtfpscr(u32 newFPSCR);
u32 PPCMfhid2(void);
void PPCMthid2(u32 newhid2);
void PPCMtwpar(u32 newwpar);
void PPCDisableSpeculation(void);
void PPCSetFpNonIEEEMode(void);
void PPCMthid4(u32 newhid4);

#ifdef __cplusplus
}
#endif

#endif
