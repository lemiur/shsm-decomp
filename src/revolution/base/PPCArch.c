#include <revolution/base.h>
#include <revolution/os.h>

/*
 * Accessors for the Broadway special-purpose registers. C cannot name
 * these registers, so most of them are asm functions in the SDK.
 */

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm u32 PPCMfmsr(void) {
    nofralloc
    mfmsr r3
    blr
} // 0x80210680

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtmsr(register u32 newMSR) {
    nofralloc
    mtmsr newMSR
    blr
} // 0x80210690

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm u32 PPCMfhid0(void) {
    nofralloc
    mfspr r3, HID0
    blr
} // 0x802106A0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMthid0(register u32 newHID0) {
    nofralloc
    mtspr HID0, newHID0
    blr
} // 0x802106B0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm u32 PPCMfl2cr(void) {
    nofralloc
    mfspr r3, L2CR
    blr
} // 0x802106C0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtl2cr(register u32 newL2cr) {
    nofralloc
    mtspr L2CR, newL2cr
    blr
} // 0x802106D0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
__declspec(weak) asm void PPCMtdec(register u32 newDec) {
    nofralloc
    mtdec newDec
    blr
} // 0x802106E0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCSync(void) {
    nofralloc
    sc
    blr
} // 0x802106F0

/*
 * PPCHalt
 *
 * Stops the program: spins forever after flushing outstanding stores.
 */
// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
__declspec(weak) asm void PPCHalt(void) {
    nofralloc
    sync
loop:
    nop
    li r3, 0
    nop
    b loop
} // 0x80210700

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtmmcr0(register u32 newMmcr0) {
    nofralloc
    mtspr MMCR0, newMmcr0
    blr
} // 0x80210720

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtmmcr1(register u32 newMmcr1) {
    nofralloc
    mtspr MMCR1, newMmcr1
    blr
} // 0x80210730

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtpmc1(register u32 newPmc1) {
    nofralloc
    mtspr PMC1, newPmc1
    blr
} // 0x80210740

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtpmc2(register u32 newPmc2) {
    nofralloc
    mtspr PMC2, newPmc2
    blr
} // 0x80210750

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtpmc3(register u32 newPmc3) {
    nofralloc
    mtspr PMC3, newPmc3
    blr
} // 0x80210760

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtpmc4(register u32 newPmc4) {
    nofralloc
    mtspr PMC4, newPmc4
    blr
} // 0x80210770

/*
 * PPCMffpscr
 *
 * Returns the FPSCR, read through f31 and the low word of a stack double.
 */
// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm u32 PPCMffpscr(void) {
    nofralloc
    stwu r1, -0x20(r1)
    stfd f31, 0x18(r1)
    mffs f31
    stfd f31, 0x8(r1)
    lfd f31, 0x18(r1)
    lwz r3, 0xC(r1)
    addi r1, r1, 0x20
    blr
} // 0x80210780

/*
 * PPCMtfpscr
 *
 * Loads the FPSCR from the low word of a stack double.
 */
// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtfpscr(register u32 newFPSCR) {
    nofralloc
    stwu r1, -0x20(r1)
    stfd f31, 0x18(r1)
    li r4, 0
    stw r4, 0x8(r1)
    stw newFPSCR, 0xC(r1)
    lfd f31, 0x8(r1)
    mtfsf 0xFF, f31
    lfd f31, 0x18(r1)
    addi r1, r1, 0x20
    blr
} // 0x802107A0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm u32 PPCMfhid2(void) {
    nofralloc
    mfspr r3, SPR_HID2
    blr
} // 0x802107D0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMthid2(register u32 newhid2) {
    nofralloc
    mtspr SPR_HID2, newhid2
    blr
} // 0x802107E0

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCMtwpar(register u32 newwpar) {
    nofralloc
    mtspr WPAR, newwpar
    blr
} // 0x802107F0

/*
 * PPCDisableSpeculation
 *
 * Sets HID0[SPD] so the core makes no speculative cache accesses.
 */
void PPCDisableSpeculation(void) {
    PPCMthid0(PPCMfhid0() | HID0_SPD);
} // 0x80210800

// ORIGINAL-ASM: SPR/MSR access; an asm function in the public RVL SDK PPCArch.c
asm void PPCSetFpNonIEEEMode(void) {
    nofralloc
    mtfsb1 29
    blr
} // 0x80210830

/*
 * PPCMthid4
 *
 * Writes HID4, refusing to clear H4A: a request without it is reported and
 * written with the bit set.
 */
// ASM-HELPER: C function whose two HID4 writes are asm statements in the public RVL SDK PPCArch.c (counted)
void PPCMthid4(register u32 newhid4) {
    if (newhid4 & HID4_H4A) {
        asm { mtspr SPR_HID4, newhid4 }
    } else {
        OSReport("H4A should not be cleared because of Broadway errata.\n");
        newhid4 |= HID4_H4A;
        asm { mtspr SPR_HID4, newhid4 }
    }
} // 0x80210840
