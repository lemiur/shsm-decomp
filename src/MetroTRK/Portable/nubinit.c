// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/nubinit.c

#include "TRK/msgbuf.h"
#include "TRK/nubevent.h"
#include "TRK/nubinit.h"
#include "TRK/serpoll.h"
#include "TRK/targimpl.h"
#include "TRK/trk_glue.h"

/* Probe word read back from the bytes 12 34 56 78: this order is a big-endian host, the reverse a little-endian one. */
#define TRK_ENDIAN_PROBE_BIG 0x12345678
#define TRK_ENDIAN_PROBE_LITTLE 0x78563412

BOOL gTRKBigEndian;

/* Brings up the nub: endianness, event queue, message buffers, serial handler, target and UART. */
DSError TRKInitializeNub(void) {
    DSError ret;
    DSError uartErr;

    ret = TRKInitializeEndian();
    if (ret == DS_NoError) {
        ret = TRKInitializeEventQueue();
    }
    if (ret == DS_NoError) {
        ret = TRKInitializeMessageBuffers();
    }
    InitializeProgramEndTrap();
    if (ret == DS_NoError) {
        ret = TRKInitializeSerialHandler();
    }
    if (ret == DS_NoError) {
        ret = TRKInitializeTarget();
    }
    if (ret == DS_NoError) {
        uartErr = TRKInitializeIntDrivenUART(1, 0, &gTRKInputPendingPtr);
        TRKTargetSetInputPendingPtr(gTRKInputPendingPtr);
        if (uartErr != DS_NoError) {
            ret = uartErr;
        }
    }
    return ret;
} // 0x80191ECC

/* Shuts the serial handler down. */
DSError TRKTerminateNub(void) {
    TRKTerminateSerialHandler();
    return DS_NoError;
} // 0x80191F78

/* Prints the nub banner on the board display. */
void TRKNubWelcome(void) {
    TRK_board_display("MetroTRK for Revolution v0.4");
} // 0x80191F9C

/* Detects the host byte order from a probe word and records it in gTRKBigEndian.
 * Returns FALSE when the probe is one of the two known orders. */
BOOL TRKInitializeEndian(void) {
    u8 bendian[4];
    BOOL result = FALSE;
    gTRKBigEndian = TRUE;

    bendian[0] = 0x12;
    bendian[1] = 0x34;
    bendian[2] = 0x56;
    bendian[3] = 0x78;

    if (*(u32*)bendian == TRK_ENDIAN_PROBE_BIG) {
        gTRKBigEndian = TRUE;
    } else if (*(u32*)bendian == TRK_ENDIAN_PROBE_LITTLE) {
        gTRKBigEndian = FALSE;
    } else {
        result = TRUE;
    }
    return result;
} // 0x80191FA8
