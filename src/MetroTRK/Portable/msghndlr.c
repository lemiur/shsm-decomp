// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/msghndlr.c

#include <revolution/os/OSError.h>
#include "TRK/dolphin_trk.h"
#include "TRK/dsmsg.h"
#include "TRK/mem_TRK.h"
#include "TRK/msg.h"
#include "TRK/msghndlr.h"
#include "TRK/msgbuf.h"
#include "TRK/nubevent.h"
#include "TRK/target_options.h"
#include "TRK/targimpl.h"
#include "TRK/uart.h"

/* Size of the head every reply starts with. */
#define TRK_REPLY_HEAD_SIZE 0x40

static BOOL IsTRKConnected;
static u32 gTRKReplySerial;

/* Reports whether a debugger host has connected. */
BOOL GetTRKConnected(void) {
    return IsTRKConnected;
} // 0x80193370

/* Sends a bare reply carrying only a command byte and an error byte. The reply serial advances twice per
 * reply: the first value is skipped, the second is stamped into the reply. */
static DSError TRKStandardACK(TRKBuffer* buffer, MessageCommandID commandID, DSReplyError replyError) {
    CommandReply reply;

    TRK_memset(&reply, 0, sizeof(CommandReply));
    gTRKReplySerial++;
    reply.commandID = commandID;
    reply.length = TRK_REPLY_HEAD_SIZE;
    reply.replyError.b = replyError;
    reply.arg.serial = gTRKReplySerial++;
    TRKWriteUARTN(&reply, sizeof(CommandReply));
    return DS_NoError;
}

/* Marks the nub connected and acknowledges the host. */
DSError TRKDoConnect(TRKBuffer* buffer) {
    IsTRKConnected = TRUE;
    return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
} // 0x80193378

/* Marks the nub disconnected, acknowledges the host and posts a shutdown event. */
DSError TRKDoDisconnect(TRKBuffer* buffer) {
    TRKEvent event;

    IsTRKConnected = FALSE;
    TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
    TRKConstructEvent(&event, NUBEVENT_Shutdown);
    TRKPostEvent(&event);
    return DS_NoError;
} // 0x801933E8

/* Acknowledges the host, then resets the system. */
DSError TRKDoReset(TRKBuffer* buffer) {
    TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
    __TRK_reset();
    return DS_NoError;
} // 0x80193470

/* Acknowledges the host, then installs the exception vectors. */
DSError TRKDoOverride(TRKBuffer* buffer) {
    TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
    __TRK_copy_vectors();
    return DS_NoError;
} // 0x801934DC

/* Reads target memory into a reply. */
DSError TRKDoReadMemory(TRKBuffer* buffer) {
    CommandRequest* request = (CommandRequest*)buffer->data;
    u8 buf[0x820] __attribute__((aligned(32)));
    size_t tempLength;
    int result;
    int replyErr;
    int options;
    size_t length;
    u32 start;

    result = DS_NoError;
    start = request->range.address;
    length = request->arg.byteCount;
    options = request->option;

    if (options & DSMSGMEMORY_Extended) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }

    if (result == DS_NoError) {
        tempLength = length;
        result = TRKTargetAccessMemory(buf, start, &tempLength, options & DSMSGMEMORY_Userview ? MEMACCESS_UserMemory : MEMACCESS_DebuggerMemory, TRUE);

        TRKResetBuffer(buffer, 0);

        if (result == DS_NoError) {
            CommandReply reply;
            TRK_memset(&reply, 0, sizeof(CommandReply));
            reply.replyError.b = result;
            reply.length = tempLength + TRK_REPLY_HEAD_SIZE;
            reply.commandID = DSMSG_ReplyACK;
            reply.arg.serial = gTRKReplySerial++;
            TRKAppendBuffer(buffer, &reply, sizeof(CommandReply));

            if (options & DSMSGMEMORY_Space_data) {
                result = TRKAppendBuffer(buffer, buf + (start & 0x1F), tempLength);
            } else {
                result = TRKAppendBuffer(buffer, buf, tempLength);
            }
        }
    }

    if (result != DS_NoError) {
        switch (result) {
        case DS_CWDSException:
            replyErr = DSREPLY_CWDSException;
            break;
        case DS_InvalidMemory:
            replyErr = DSREPLY_InvalidMemoryRange;
            break;
        case DS_InvalidProcessID:
            replyErr = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyErr = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyErr = DSREPLY_OSError;
            break;
        default:
            replyErr = DSREPLY_CWDSError;
            break;
        }
        return TRKStandardACK(buffer, DSMSG_ReplyACK, replyErr);
    }

    return TRKMessageSend(buffer);
} // 0x80193548

/* Writes the request's data to target memory. */
DSError TRKDoWriteMemory(TRKBuffer* buffer) {
    CommandRequest* request = (CommandRequest*)buffer->data;
    u8 buf[0x820] __attribute__((aligned(32)));
    size_t tempLength;
    int result;
    int replyErr;
    int options;
    size_t length;
    u32 start;

    result = DS_NoError;
    start = request->range.address;
    length = request->arg.byteCount;
    options = request->option;

    if (options & DSMSGMEMORY_Extended) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }

    if (result == DS_NoError) {
        tempLength = length;

        TRKSetBufferPosition(buffer, TRK_REPLY_HEAD_SIZE);
        TRKReadBuffer(buffer, buf, tempLength);
        result = TRKTargetAccessMemory(buf, start, &tempLength, options & DSMSGMEMORY_Userview ? MEMACCESS_UserMemory : MEMACCESS_DebuggerMemory, FALSE);

        TRKResetBuffer(buffer, 0);

        if (result == DS_NoError) {
            CommandReply reply;
            TRK_memset(&reply, 0, sizeof(CommandReply));
            reply.length = TRK_REPLY_HEAD_SIZE;
            reply.commandID = DSMSG_ReplyACK;
            reply.replyError.b = result;
            reply.arg.serial = gTRKReplySerial++;
            result = TRKAppendBuffer(buffer, &reply, sizeof(CommandReply));
        }
    }

    if (result != DS_NoError) {
        switch (result) {
        case DS_CWDSException:
            replyErr = DSREPLY_CWDSException;
            break;
        case DS_InvalidMemory:
            replyErr = DSREPLY_InvalidMemoryRange;
            break;
        case DS_InvalidProcessID:
            replyErr = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyErr = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyErr = DSREPLY_OSError;
            break;
        default:
            replyErr = DSREPLY_CWDSError;
            break;
        }
        return TRKStandardACK(buffer, DSMSG_ReplyACK, replyErr);
    }

    return TRKMessageSend(buffer);
} // 0x8019377C

/* Reads the register blocks (default, FP, extended 1, extended 2) into a reply. */
DSError TRKDoReadRegisters(TRKBuffer* buffer) {
    CommandRequest* request = (CommandRequest*)buffer->data;
    int error;
    u16 firstRegister;
    u16 lastRegister;
    size_t registersLength;
    CommandReply reply;

    firstRegister = request->arg.firstRegister;
    lastRegister = request->range.lastRegister;

    if (firstRegister > lastRegister) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_InvalidRegisterRange);
    }

    /* The original leaves the rest of the head uninitialised (stack contents). */
    reply.commandID = DSMSG_ReplyACK;
    reply.length = 0x468;
    reply.arg.serial = gTRKReplySerial++;

    TRKResetBuffer(buffer, 0);

    TRKAppendBuffer_ui8(buffer, (u8*)&reply, sizeof(CommandReply));

    error = TRKTargetAccessDefault(0, 36, buffer, &registersLength, TRUE);
    if (error == DS_NoError) {
        error = TRKTargetAccessFP(0, 33, buffer, &registersLength, TRUE);
    }
    if (error == DS_NoError) {
        error = TRKTargetAccessExtended1(0, 0x60, buffer, &registersLength, TRUE);
    }
    if (error == DS_NoError) {
        error = TRKTargetAccessExtended2(0, 31, buffer, &registersLength, TRUE);
    }

    if (error != DS_NoError) {
        int replyError;
        switch (error) {
        case DS_UnsupportedError:
            replyError = DSREPLY_UnsupportedOptionError;
            break;
        case DS_InvalidRegister:
            replyError = DSREPLY_InvalidRegisterRange;
            break;
        case DS_CWDSException:
            replyError = DSREPLY_CWDSException;
            break;
        case DS_InvalidProcessID:
            replyError = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyError = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyError = DSREPLY_OSError;
            break;
        default:
            replyError = DSREPLY_CWDSError;
        }

        return TRKStandardACK(buffer, DSMSG_ReplyACK, replyError);
    } else {
        return TRKMessageSend(buffer);
    }
} // 0x8019398C

/* Writes one register block from the request's data. */
DSError TRKDoWriteRegisters(TRKBuffer* buffer) {
    CommandRequest* request = (CommandRequest*)buffer->data;
    int error;
    int replyError;
    u8 options;
    u16 firstRegister;
    u16 lastRegister;
    size_t registersLength;

    options = request->option;
    firstRegister = request->arg.firstRegister;
    lastRegister = request->range.lastRegister;

    TRKSetBufferPosition(buffer, 0);

    if (firstRegister > lastRegister) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_InvalidRegisterRange);
    }

    TRKSetBufferPosition(buffer, TRK_REPLY_HEAD_SIZE);

    switch (options) {
    case DSREG_Default:
        error = TRKTargetAccessDefault(firstRegister, lastRegister, buffer, &registersLength, FALSE);
        break;
    case DSREG_FP:
        error = TRKTargetAccessFP(firstRegister, lastRegister, buffer, &registersLength, FALSE);
        break;
    case DSREG_Extended1:
        error = TRKTargetAccessExtended1(firstRegister, lastRegister, buffer, &registersLength, FALSE);
        break;
    case DSREG_Extended2:
        error = TRKTargetAccessExtended2(firstRegister, lastRegister, buffer, &registersLength, FALSE);
        break;
    default:
        error = DS_UnsupportedError;
        break;
    }

    TRKResetBuffer(buffer, 0);

    if (error == DS_NoError) {
        CommandReply reply;
        TRK_memset(&reply, 0, sizeof(CommandReply));
        reply.length = TRK_REPLY_HEAD_SIZE;
        reply.commandID = DSMSG_ReplyACK;
        reply.replyError.b = error;
        reply.arg.serial = gTRKReplySerial++;
        error = TRKAppendBuffer(buffer, &reply, sizeof(CommandReply));
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_UnsupportedError:
            replyError = DSREPLY_UnsupportedOptionError;
            break;
        case DS_InvalidRegister:
            replyError = DSREPLY_InvalidRegisterRange;
            break;
        case DS_MessageBufferReadError:
            replyError = DSREPLY_PacketSizeError;
            break;
        case DS_CWDSException:
            replyError = DSREPLY_CWDSException;
            break;
        case DS_InvalidProcessID:
            replyError = DSREPLY_InvalidProcessID;
            break;
        case DS_InvalidThreadID:
            replyError = DSREPLY_InvalidThreadID;
            break;
        case DS_OSError:
            replyError = DSREPLY_OSError;
            break;
        default:
            replyError = DSREPLY_CWDSError;
        }

        return TRKStandardACK(buffer, DSMSG_ReplyACK, replyError);
    } else {
        return TRKMessageSend(buffer);
    }
} // 0x80193B88

/* Resumes the stopped target. */
DSError TRKDoContinue(TRKBuffer* buffer) {
    if (!TRKTargetStopped()) {
        TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NotStopped);
        return DS_NoError;
    } else {
        TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
        return TRKTargetContinue();
    }
} // 0x80193E28

/* Single-steps the stopped target, by count or until it leaves a range. */
DSError TRKDoStep(TRKBuffer* buffer) {
    CommandRequest* request = (CommandRequest*)buffer->data;
    DSError result;
    u8 options;
    u8 count;
    u32 rangeStart;
    u32 rangeEnd;
    u32 pc;

    TRKSetBufferPosition(buffer, 0);

    options = request->option;
    rangeStart = request->range.rangeStart;
    rangeEnd = request->rangeEnd;

    switch (options) {
    case DSSTEP_IntoCount:
    case DSSTEP_OverCount:
        count = request->arg.stepCount;
        if (count >= 1) {
            break;
        }
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_ParameterError);
    case DSSTEP_IntoRange:
    case DSSTEP_OverRange:
        pc = TRKTargetGetPC();
        if (pc >= rangeStart && pc <= rangeEnd) {
            break;
        }
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_ParameterError);
    default:
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }

    if (!TRKTargetStopped()) {
        return TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NotStopped);
    } else {
        result = TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);
        switch (options) {
        case DSSTEP_IntoCount:
        case DSSTEP_OverCount:
            result = TRKTargetSingleStep(count, (options == DSSTEP_OverCount));
            break;
        case DSSTEP_IntoRange:
        case DSSTEP_OverRange:
            result = TRKTargetStepOutOfRange(rangeStart, rangeEnd, (options == DSSTEP_OverRange));
            break;
        }

        return result;
    }
} // 0x80193EEC

/* Stops the target and acknowledges with the matching error. */
DSError TRKDoStop(TRKBuffer* buffer) {
    DSReplyError replyError;

    switch (TRKTargetStop()) {
    case DS_NoError:
        replyError = DSREPLY_NoError;
        break;
    case DS_InvalidProcessID:
        replyError = DSREPLY_InvalidProcessID;
        break;
    case DS_InvalidThreadID:
        replyError = DSREPLY_InvalidThreadID;
        break;
    case DS_OSError:
        replyError = DSREPLY_OSError;
        break;
    default:
        replyError = DSREPLY_Error;
        break;
    }

    TRKStandardACK(buffer, DSMSG_ReplyACK, replyError);

    return DS_NoError;
} // 0x80194170

/* Handles an option request; option 1 switches serial IO on or off. */
DSError TRKDoSetOption(TRKBuffer* buffer) {
    CommandRequest* request = (CommandRequest*)buffer->data;
    u8 enable = request->arg.enable;

    if (request->option == 1) {
        OSReport("\nMetroTRK Option : SerialIO - ");
        if (enable) {
            OSReport("Enable\n");
        } else {
            OSReport("Disable\n");
        }
        SetUseSerialIO(enable);
    }

    TRKStandardACK(buffer, DSMSG_ReplyACK, DSREPLY_NoError);

    return DS_NoError;
} // 0x80194228
