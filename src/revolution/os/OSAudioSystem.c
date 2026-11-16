#include <revolution/os.h>
#include <revolution/hardware.h>
#include <string.h>

/* __DSPRegs halfword indices. The DMA address and length registers are 32-bit pairs. */
#define DSP_MAILBOX_IN_HI  0 /* CPU -> DSP mailbox */
#define DSP_MAILBOX_OUT_HI 2 /* DSP -> CPU mailbox */
#define DSP_MAILBOX_OUT_LO 3
#define DSP_CONTROL_STATUS 5
#define DSP_AR_SIZE        9
#define DSP_AR_DMA_CNT_H   27

#define DSP_DMA_MMADDR (*(vu32*)&__DSPRegs[16]) // volatile: hardware register
#define DSP_DMA_ARADDR (*(vu32*)&__DSPRegs[18]) // volatile: hardware register
#define DSP_DMA_CNT    (*(vu32*)&__DSPRegs[20]) // volatile: hardware register

/* __ACRRegs word indices (invented names). */
#define ACR_AUDIO_CLOCK (0x180 / 4)
#define ACR_AI_PLL_LO   (0x1CC / 4)
#define ACR_AI_PLL_HI   (0x1D0 / 4)

/* Replaces the bits of reg under mask with val shifted into place (invented name). */
#define SET_FIELD(reg, mask, shift, val) (((reg) & ~(mask)) | ((val) << (shift)))

/* Main-memory buffer the DSP boot code is copied through. */
#define __DSPWorkBuffer ((void*)0x81000000)

/* Busy-waits for usec microseconds (invented name). */
#define WAIT_MICROSECONDS(usec)                                       \
    {                                                                 \
        OSTick start = OSGetTick();                                   \
        while (OSTicksToMicroseconds(OSGetTick() - start) < (usec)) { \
        }                                                             \
    }

/* DSP microcode that the boot sequence runs to reset the DSP. */
static u8 DSPInitCode[128] = {
    0x02, 0x9F, 0x00, 0x10, 0x02, 0x9F, 0x00, 0x33, 0x02, 0x9F, 0x00, 0x34, 0x02, 0x9F, 0x00, 0x35,
    0x02, 0x9F, 0x00, 0x36, 0x02, 0x9F, 0x00, 0x37, 0x02, 0x9F, 0x00, 0x38, 0x02, 0x9F, 0x00, 0x39,
    0x12, 0x06, 0x12, 0x03, 0x12, 0x04, 0x12, 0x05, 0x00, 0x80, 0x80, 0x00, 0x00, 0x88, 0xFF, 0xFF,
    0x00, 0x84, 0x10, 0x00, 0x00, 0x64, 0x00, 0x1D, 0x02, 0x18, 0x00, 0x00, 0x81, 0x00, 0x1C, 0x1E,
    0x00, 0x44, 0x1B, 0x1E, 0x00, 0x84, 0x08, 0x00, 0x00, 0x64, 0x00, 0x27, 0x19, 0x1E, 0x00, 0x00,
    0x00, 0xDE, 0xFF, 0xFC, 0x02, 0xA0, 0x80, 0x00, 0x02, 0x9C, 0x00, 0x28, 0x16, 0xFC, 0x00, 0x54,
    0x16, 0xFD, 0x43, 0x48, 0x00, 0x21, 0x02, 0xFF, 0x02, 0xFF, 0x02, 0xFF, 0x02, 0xFF, 0x02, 0xFF,
    0x02, 0xFF, 0x02, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * __AIClockInit
 *
 * Programs the audio clock PLL for a 48 kHz or a 32 kHz output rate.
 */
static void __AIClockInit(BOOL is48k) {
    u32 reg;

    reg = __ACRRegs[ACR_AUDIO_CLOCK];
    reg &= ~0x100;
    reg |= is48k << 8;
    reg &= ~0x80;
    __ACRRegs[ACR_AUDIO_CLOCK] = reg;

    __ACRRegs[ACR_AI_PLL_HI] &= ~0xC0000000;
    WAIT_MICROSECONDS(100);

    if (!is48k) {
        reg = __ACRRegs[ACR_AI_PLL_LO];
        reg = SET_FIELD(reg, 0x3FFC0, 6, 0x3F);
        reg = SET_FIELD(reg, 0x3F, 0, 0);
        reg = SET_FIELD(reg, 0x7FC0000, 18, 0x119);
        __ACRRegs[ACR_AI_PLL_LO] = reg;
    } else {
        reg = __ACRRegs[ACR_AI_PLL_LO];
        reg = SET_FIELD(reg, 0x3FFC0, 6, 0x3FF);
        reg = SET_FIELD(reg, 0x3F, 0, 0xE);
        reg = SET_FIELD(reg, 0x7FC0000, 18, 0x12C);
        __ACRRegs[ACR_AI_PLL_LO] = reg;
    }
    WAIT_MICROSECONDS(100);

    __ACRRegs[ACR_AI_PLL_HI] &= ~0x10000000;
    WAIT_MICROSECONDS(1000);

    __ACRRegs[ACR_AI_PLL_HI] = SET_FIELD(__ACRRegs[ACR_AI_PLL_HI], 0x40000000, 30, 1);
    WAIT_MICROSECONDS(1000);

    __ACRRegs[ACR_AI_PLL_HI] = SET_FIELD(__ACRRegs[ACR_AI_PLL_HI], 0x80000000, 31, 1);
    WAIT_MICROSECONDS(1000);
} // 0x80212AC0

/*
 * __OSInitAudioSystem
 *
 * Boots the DSP with DSPInitCode, saving the main-memory bytes the boot
 * code overwrites at the top of the arena.
 */
void __OSInitAudioSystem(void) {
    u16 reg16;
    OSTick start;

    if (!__OSInIPL) {
        __AIClockInit(TRUE);
    }

    memcpy((u8*)OSGetArenaHi() - 128, __DSPWorkBuffer, 128);
    memcpy(__DSPWorkBuffer, DSPInitCode, 128);
    DCFlushRange(__DSPWorkBuffer, 128);

    __DSPRegs[DSP_AR_SIZE] = 0x43;
    __DSPRegs[DSP_CONTROL_STATUS] = 0x8AC;
    __DSPRegs[DSP_CONTROL_STATUS] |= 1;
    while (__DSPRegs[DSP_CONTROL_STATUS] & 1) {
    }

    __DSPRegs[DSP_MAILBOX_IN_HI] = 0;
    while (((__DSPRegs[DSP_MAILBOX_OUT_HI] << 16) | __DSPRegs[DSP_MAILBOX_OUT_LO]) & 0x80000000) {
    }

    DSP_DMA_MMADDR = 0x1000000;
    DSP_DMA_ARADDR = 0;
    DSP_DMA_CNT = 32;
    reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    while (!(reg16 & 0x20)) {
        reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    }
    __DSPRegs[DSP_CONTROL_STATUS] = reg16;

    start = OSGetTick();
    while ((s32)(OSGetTick() - start) < 0x892) {
    }

    DSP_DMA_MMADDR = 0x1000000;
    DSP_DMA_ARADDR = 0;
    DSP_DMA_CNT = 32;
    reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    while (!(reg16 & 0x20)) {
        reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    }
    __DSPRegs[DSP_CONTROL_STATUS] = reg16;

    __DSPRegs[DSP_CONTROL_STATUS] &= ~0x800;
    while (__DSPRegs[DSP_CONTROL_STATUS] & 0x400) {
    }

    __DSPRegs[DSP_CONTROL_STATUS] &= ~4;
    reg16 = __DSPRegs[DSP_MAILBOX_OUT_HI];
    while (!(reg16 & 0x8000)) {
        reg16 = __DSPRegs[DSP_MAILBOX_OUT_HI];
    }
    (void)__DSPRegs[DSP_MAILBOX_OUT_LO];

    __DSPRegs[DSP_CONTROL_STATUS] |= 4;
    __DSPRegs[DSP_CONTROL_STATUS] = 0x8AC;
    __DSPRegs[DSP_CONTROL_STATUS] |= 1;
    while (__DSPRegs[DSP_CONTROL_STATUS] & 1) {
    }

    memcpy(__DSPWorkBuffer, (u8*)OSGetArenaHi() - 128, 128);
} // 0x80212CE0

/*
 * __OSStopAudioSystem
 *
 * Halts the DSP and waits for its DMA to drain.
 */
void __OSStopAudioSystem(void) {
    u16 reg16;
    OSTick start;

    __DSPRegs[DSP_CONTROL_STATUS] = 0x804;
    __DSPRegs[DSP_AR_DMA_CNT_H] &= ~0x8000;
    reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    while (reg16 & 0x400) {
        reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    }
    reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    while (reg16 & 0x200) {
        reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    }

    __DSPRegs[DSP_CONTROL_STATUS] = 0x8AC;
    __DSPRegs[DSP_MAILBOX_IN_HI] = 0;
    while (((__DSPRegs[DSP_MAILBOX_OUT_HI] << 16) | __DSPRegs[DSP_MAILBOX_OUT_LO]) & 0x80000000) {
    }

    start = OSGetTick();
    while ((s32)(OSGetTick() - start) < 0x2C) {
    }

    __DSPRegs[DSP_CONTROL_STATUS] |= 1;
    reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    while (reg16 & 1) {
        reg16 = __DSPRegs[DSP_CONTROL_STATUS];
    }
} // 0x80212EB0
