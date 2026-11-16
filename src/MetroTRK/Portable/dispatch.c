// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/dispatch.c

#include "TRK/dispatch.h"
#include "TRK/dsmsg.h"
#include "TRK/msghndlr.h"

/* Routes a received request to the handler of its command byte (data[4]). A command with no
 * handler leaves DS_DispatchError as the result. */
BOOL TRKDispatchMessage(TRKBuffer* msg) {
    DSError err;

    err = DS_DispatchError;
    TRKSetBufferPosition(msg, 0);

    switch (msg->data[4]) {
    case DSMSG_Connect:
        err = TRKDoConnect(msg);
        break;
    case DSMSG_Disconnect:
        err = TRKDoDisconnect(msg);
        break;
    case DSMSG_Reset:
        err = TRKDoReset(msg);
        break;
    case DSMSG_Override:
        err = TRKDoOverride(msg);
        break;
    case DSMSG_ReadMemory:
        err = TRKDoReadMemory(msg);
        break;
    case DSMSG_WriteMemory:
        err = TRKDoWriteMemory(msg);
        break;
    case DSMSG_ReadRegisters:
        err = TRKDoReadRegisters(msg);
        break;
    case DSMSG_WriteRegisters:
        err = TRKDoWriteRegisters(msg);
        break;
    case DSMSG_Continue:
        err = TRKDoContinue(msg);
        break;
    case DSMSG_Step:
        err = TRKDoStep(msg);
        break;
    case DSMSG_Stop:
        err = TRKDoStop(msg);
        break;
    case DSMSG_SetOption:
        err = TRKDoSetOption(msg);
        break;
    }
    return err;
} // 0x801914C0
