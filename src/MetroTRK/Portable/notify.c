// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/notify.c
// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MetroTRK/notify.c

#include "TRK/notify.h"
#include "TRK/msgbuf.h"
#include "TRK/support.h"
#include "TRK/targimpl.h"

/* Tells the host the target stopped (cmd NotifyStopped) or took an exception (anything else). */
DSError TRKDoNotifyStopped(MessageCommandID cmd) {
    MessageBufferID replyID;
    MessageBufferID bufferID;
    TRKBuffer* buffer;
    DSError result;

    result = TRKGetFreeBuffer(&bufferID, &buffer);

    if (result == DS_NoError) {
        if (cmd == DSMSG_NotifyStopped) {
            TRKTargetAddStopInfo(buffer);
        } else {
            TRKTargetAddExceptionInfo(buffer);
        }

        result = TRKRequestSend(buffer, &replyID);

        if (result == DS_NoError) {
            TRKReleaseBuffer(replyID);
        }

        TRKReleaseBuffer(bufferID);
    }

    return result;
} // 0x80191CA0
