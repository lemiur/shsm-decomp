// Reference: zeldaret/tp libs/revolution/src/wpad/WPADMem.c
// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/wpad/R4QK01/WPADMem.c (function names)
//
// Retail is the Jun 2009 SDK: the pending memory access of a channel (tp's global _wmb array) lives in the control
// block (WPADControlBlock::memBlock), and the unit carries the two functions below.
#include <revolution/wpad/__wpad.h>

// Marks the channel's pending memory access as free. Name from the mscharged R4QK01 reference.
void WPADiClearMemBlock(s32 chan) {
    WPADControlBlock* cb = _wpdcb[chan];

    cb->memBlock.used = FALSE;
    cb->memBlock.buf = NULL;
    cb->memBlock.length = 0;
    cb->memBlock.address = 0;
    cb->memBlock.callback = NULL;
} // 0x8028A370

// Writes to a register of the extension controller (I2C device 0xA4). Name from the mscharged R4QK01 reference. Callback reports a failed or refused queueing.
s32 WPADWriteExtReg(s32 chan, void* buf, u16 len, u16 addr, WPADCallback callback) {
    WPADControlBlock* cb = _wpdcb[chan];
    BOOL enabled;
    BOOL setup;
    s32 status;

    enabled = OSDisableInterrupts();
    status = cb->status;
    setup = cb->handshakeFinished;
    OSRestoreInterrupts(enabled);

    if (status != WPAD_ERR_NO_CONTROLLER) {
        if (!setup) {
            status = WPAD_ERR_BUSY;
        } else if (WPADiSendWriteData(&cb->stdCmdQueue, buf, len, WM_EXT_REG_ADDR(EXTENSION, addr), callback)) {
            status = WPAD_ERR_OK;
        } else {
            status = WPAD_ERR_BUSY;
        }
    }

    if (status != WPAD_ERR_OK && callback != NULL) {
        callback(chan, status);
    }
    return status;
} // 0x8028A3A0
