// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/main_TRK.c

#include "TRK/dserror.h"
#include "TRK/main_TRK.h"
#include "TRK/mainloop.h"
#include "TRK/nubinit.h"

static DSError TRK_mainError;

/* Entry point of the MetroTRK nub: brings it up, runs the main loop and tears it down. */
DSError TRK_main(void) {
    TRK_mainError = TRKInitializeNub();

    if (TRK_mainError == DS_NoError) {
        TRKNubWelcome();
        TRKNubMainLoop();
    }

    TRK_mainError = TRKTerminateNub();
    return TRK_mainError;
} // 0x80191134
