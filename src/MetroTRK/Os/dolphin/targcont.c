// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Os/dolphin/targcont.c

#include "TRK/targimpl.h"
#include "TRK/trk_glue.h"

/* Resumes the target: clears its stopped state, releases the EXI2 port, runs the target until it stops
 * again, then reserves the port back. */
DSError TRKTargetContinue(void) {
    TRKTargetSetStopped(0);
    UnreserveEXI2Port();
    TRKSwapAndGo();
    ReserveEXI2Port();
    return DS_NoError;
} // 0x801927A4
