// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/mainloop.c

#include "TRK/dispatch.h"
#include "TRK/mainloop.h"
#include "TRK/msgbuf.h"
#include "TRK/nubevent.h"
#include "TRK/serpoll.h"
#include "TRK/targimpl.h"

/* Main loop of the nub: handles queued events, polls for input and resumes the target
 * until a shutdown event arrives. */
void TRKNubMainLoop(void) {
    void* msg;
    TRKEvent event;
    BOOL isShutdownRequested;
    BOOL isNewInput;

    isShutdownRequested = FALSE;
    isNewInput = FALSE;
    while (isShutdownRequested == FALSE) {
        if (TRKGetNextEvent(&event) != FALSE) {
            isNewInput = FALSE;

            switch (event.eventType) {
            case NUBEVENT_Null:
                break;

            case NUBEVENT_Request:
                msg = TRKGetBuffer(event.msgBufID);
                TRKDispatchMessage(msg);
                break;

            case NUBEVENT_Shutdown:
                isShutdownRequested = TRUE;
                break;

            case NUBEVENT_Exception:
            case NUBEVENT_Breakpoint:
                TRKTargetInterrupt(&event);
                break;

            case NUBEVENT_Support:
                TRKTargetSupportRequest();
                break;
            }

            TRKDestructEvent(&event);
            continue;
        }

        if ((isNewInput == FALSE) || (*(u8*)gTRKInputPendingPtr != '\0')) {
            isNewInput = TRUE;
            TRKGetInput();
            continue;
        }

        if (TRKTargetStopped() == FALSE) {
            TRKTargetContinue();
        }
        isNewInput = FALSE;
    }
} // 0x80191170
