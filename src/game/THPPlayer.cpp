// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/THPPlayer.h>
#include <revolution/os/OSCache.h>
#include <revolution/os/OSInterrupt.h>
#include <revolution/os/OSThread.h>
#include <revolution/os/OSTime.h>

/*
====================
THPPlayerQuit
====================
*/
// 0x8009A850
extern "C" void THPPlayerQuit() {
    LCDisable();
    BOOL enabled = OSDisableInterrupts();
    if (lbl_80529788 != NULL) {
        AIRegisterDMACallback(lbl_80529788);
    }
    OSRestoreInterrupts(enabled);
    lbl_80529778 = 0;
}

/*
====================
fn_8009A8A4

Starts an asynchronous DVD read, then polls the drive every 10 ms until it leaves the busy
state or two seconds have passed. A read still busy at the timeout is reported as NO_DISK.
Returns the length on success and -1 on failure, cancelling the read on failure.
====================
*/
// 0x8009A8A4
extern "C" s32 fn_8009A8A4(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset) {
    s32 result = 0;
    s32 elapsed = 0;
    s32 state;

    DVDReadAsyncPrio(fileInfo, addr, length, offset, NULL, 2);
    do {
        // The 10 ms tick count is multiplied as an OSTime (64-bit), as in retail.
        OSSleepTicks(OSMillisecondsToTicks((OSTime)10));
        elapsed += 10;
        state = DVDGetDriveStatus();
        if (elapsed > 2000) {
            state = DVD_STATE_NO_DISK;
        }
    } while (state == DVD_STATE_BUSY);

    switch (state) {
    case DVD_STATE_FATAL_ERROR:
        fn_8018F788(2, 0);
        result = -1;
        break;
    case DVD_STATE_NO_DISK:
        fn_8018F788(3, 0);
        result = -1;
        break;
    case DVD_STATE_COVER_OPEN:
        fn_8018F788(4, 0);
        result = -1;
        break;
    case DVD_STATE_WRONG_DISK:
        fn_8018F788(5, 0);
        result = -1;
        break;
    case DVD_STATE_RETRY:
        fn_8018F788(6, 0);
        result = -1;
        break;
    case DVD_STATE_BUSY:
        // Not reachable after the poll loop, which exits only when the state is not BUSY; retail keeps the case.
        break;
    case DVD_STATE_END:
        result = length;
        break;
    default:
        fn_8018F788(7, 0);
        result = -1;
        break;
    }

    if (result == -1) {
        DVDCancel(&fileInfo->cb);
    }
    return result;
}
