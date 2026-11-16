// Reference: doldecomp/ogws src/revolution/PAD/Pad.c (the retail SDK links only this function and __PADSpec)
#include <revolution/os.h>
#include <revolution/pad.h>

/* Bit of the low-memory PAD flag byte that stops the controllers from recalibrating. */
#define PAD_FLAG_NO_RECALIBRATE 0x40

/* Low-memory byte shared with the system menu. */
#define OS_PAD_FLAGS (*(u8*)0x800030E3)

u32 __PADSpec;

/*
 * __PADDisableRecalibration
 *
 * Sets or clears the "do not recalibrate" flag and returns its previous state.
 */
BOOL __PADDisableRecalibration(BOOL disable) {
    BOOL enabled = OSDisableInterrupts();
    BOOL old = (OS_PAD_FLAGS & PAD_FLAG_NO_RECALIBRATE) ? TRUE : FALSE;

    OS_PAD_FLAGS &= ~PAD_FLAG_NO_RECALIBRATE;

    if (disable) {
        OS_PAD_FLAGS |= PAD_FLAG_NO_RECALIBRATE;
    }

    OSRestoreInterrupts(enabled);
    return old;
} // 0x8022CA00
