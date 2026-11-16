// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/support.c

#include <revolution/os/OSError.h>
#include "TRK/mem_TRK.h"
#include "TRK/msg.h"
#include "TRK/msgbuf.h"
#include "TRK/serpoll.h"
#include "TRK/string_TRK.h"
#include "TRK/support.h"

/* Reads or writes a host file in chunks of at most 0x800 bytes, one request per chunk. */
DSError TRKSuppAccessFile(u32 file_handle, u8* data, size_t* count, DSIOResult* io_result, BOOL need_reply, BOOL read) {
    DSError error;
    MessageBufferID replyBufferId;
    TRKBuffer* replyBuffer;
    u32 length;
    MessageBufferID bufferId;
    TRKBuffer* buffer;
    u32 i;
    u8 replyIOResult;
    u32 replyLength;
    BOOL exit;
    CommandReply reply;

    if (data == NULL || *count == 0) {
        return DS_ParameterError;
    }

    exit = FALSE;
    *io_result = DS_IONoError;
    i = 0;
    error = DS_NoError;
    while (!exit && i < *count && error == DS_NoError && *io_result == DS_IONoError) {
        TRK_memset(&reply, 0, sizeof(CommandReply));

        if (*count - i <= 0x800) {
            length = *count - i;
        } else {
            length = 0x800;
        }

        reply.commandID = read ? DSMSG_ReadFile : DSMSG_WriteFile;

        if (read) {
            reply.length = 0x40;
        } else {
            reply.length = length + 0x40;
        }

        reply.replyError.r = file_handle;
        reply.arg.count = length;

        TRKGetFreeBuffer(&bufferId, &buffer);
        error = TRKAppendBuffer_ui8(buffer, (const u8*)&reply, 0x40);

        if (!read && error == DS_NoError) {
            error = TRKAppendBuffer_ui8(buffer, data + i, length);
        }

        if (error == DS_NoError) {
            if (need_reply) {
                error = TRKRequestSend(buffer, &replyBufferId);
                if (error == DS_NoError) {
                    replyBuffer = TRKGetBuffer(replyBufferId);
                }
                replyIOResult = ((CommandReply*)replyBuffer->data)->param.ioResult;
                replyLength = ((CommandReply*)replyBuffer->data)->replyLength;
                if (read && error == DS_NoError && replyLength <= length) {
                    TRKSetBufferPosition(replyBuffer, 0x40);
                    error = TRKReadBuffer_ui8(replyBuffer, data + i, replyLength);
                    if (error == DS_MessageBufferReadError) {
                        error = DS_NoError;
                    }
                }

                if (replyLength != length) {
                    length = replyLength;
                    exit = TRUE;
                }

                *io_result = (DSIOResult)replyIOResult;
                TRKReleaseBuffer(replyBufferId);
            } else {
                error = TRKMessageSend(buffer);
            }
        }

        TRKReleaseBuffer(bufferId);
        i += length;
    }

    *count = i;
    return error;
} // 0x80192180

/* Sends a message and waits for its reply; host requests that arrive meanwhile go to TRKProcessInput. */
DSError TRKRequestSend(TRKBuffer* msgBuf, MessageBufferID* bufferId) {
    DSError error;
    TRKBuffer* buffer;
    u8 msgCmd;
    u8 msgReplyError;
    BOOL badReply;

    error = TRKMessageSend(msgBuf);
    if (error == DS_NoError) {
        badReply = FALSE;
        while (TRUE) {
            do {
                *bufferId = TRKTestForPacket();
            } while (*bufferId == -1);

            buffer = TRKGetBuffer(*bufferId);
            TRKSetBufferPosition(buffer, 0);
            msgCmd = ((CommandReply*)buffer->data)->commandID;
            if (msgCmd >= DSMSG_ReplyACK) {
                break;
            }

            TRKProcessInput(*bufferId);
            *bufferId = -1;
        }

        if (*bufferId != -1) {
            if (buffer->length < 0x40) {
                OSReport("MetroTRK - bad reply size %ld\n", buffer->length);
                badReply = TRUE;
            }
            if (error == DS_NoError && !badReply) {
                msgReplyError = ((CommandReply*)buffer->data)->replyError.b;
            }
            if (error == DS_NoError && !badReply) {
                if ((int)msgCmd != DSMSG_ReplyACK || msgReplyError != DSREPLY_NoError) {
                    badReply = TRUE;
                }
            }
            if (error != DS_NoError || badReply) {
                TRKReleaseBuffer(*bufferId);
                *bufferId = -1;
            }
        }
    }

    if (*bufferId == -1) {
        OSReport("MetroTRK - failed in RequestSend\n");
        error = DS_Error800;
    }
    return error;
} // 0x80192374

/* Sends an open request for a path; the reply's file handle comes back through fileHandle. */
DSError HandleOpenFileSupportRequest(const char* path, u8 replyError, u32* fileHandle, DSIOResult* ioResult) {
    DSError error;
    MessageBufferID bufferId2;
    MessageBufferID bufferId1;
    TRKBuffer* tempBuffer;
    TRKBuffer* buffer;
    CommandReply reply;

    TRK_memset(&reply, 0, sizeof(CommandReply));
    *fileHandle = 0;
    reply.commandID = DSMSG_OpenFile;
    reply.length = TRK_strlen(path) + 0x40 + 1;
    reply.replyError.b = replyError;
    reply.arg.count = TRK_strlen(path) + 1;

    TRKGetFreeBuffer(&bufferId1, &buffer);
    error = TRKAppendBuffer_ui8(buffer, (const u8*)&reply, 0x40);

    if (error == DS_NoError) {
        error = TRKAppendBuffer_ui8(buffer, (const u8*)path, TRK_strlen(path) + 1);
    }

    if (error == DS_NoError) {
        *ioResult = DS_IONoError;
        error = TRKRequestSend(buffer, &bufferId2);

        if (error == DS_NoError) {
            tempBuffer = TRKGetBuffer(bufferId2);
        }

        *ioResult = ((CommandReply*)tempBuffer->data)->param.ioResult;
        *fileHandle = ((CommandReply*)tempBuffer->data)->replyError.r;
        TRKReleaseBuffer(bufferId2);
    }
    TRKReleaseBuffer(bufferId1);
    return error;
} // 0x8019249C

/* Sends a close request for a file handle and reads back the IO result. */
DSError HandleCloseFileSupportRequest(int replyError, DSIOResult* ioResult) {
    DSError error;
    MessageBufferID replyBufferId;
    MessageBufferID bufferId;
    TRKBuffer* buffer1;
    TRKBuffer* buffer2;
    CommandReply reply;

    TRK_memset(&reply, 0, sizeof(CommandReply));
    reply.commandID = DSMSG_CloseFile;
    reply.length = 0x40;
    reply.replyError.r = replyError;
    error = TRKGetFreeBuffer(&bufferId, &buffer1);

    if (error == DS_NoError) {
        error = TRKAppendBuffer_ui8(buffer1, (const u8*)&reply, sizeof(CommandReply));
    }

    if (error == DS_NoError) {
        *ioResult = DS_IONoError;
        error = TRKRequestSend(buffer1, &replyBufferId);

        if (error == DS_NoError) {
            buffer2 = TRKGetBuffer(replyBufferId);
        }

        if (error == DS_NoError) {
            *ioResult = ((CommandReply*)buffer2->data)->param.ioResult;
        }

        TRKReleaseBuffer(replyBufferId);
    }

    TRKReleaseBuffer(bufferId);
    return error;
} // 0x801925B4

/* Sends a position request for a file handle and reads back the new position and the IO result. */
DSError HandlePositionFileSupportRequest(DSReplyError replyErr, u32* position, u8 seekMode, DSIOResult* ioResult) {
    DSError error;
    MessageBufferID bufferId2;
    MessageBufferID bufferId1;
    TRKBuffer* buffer1;
    TRKBuffer* buffer2;
    CommandReply reply;

    TRK_memset(&reply, 0, sizeof(CommandReply));
    reply.commandID = DSMSG_PositionFile;
    reply.length = 0x40;
    reply.replyError.r = replyErr;
    reply.arg.offset = *position;
    reply.param.seekMode = seekMode;
    error = TRKGetFreeBuffer(&bufferId1, &buffer1);

    if (error == DS_NoError) {
        error = TRKAppendBuffer_ui8(buffer1, (const u8*)&reply, sizeof(CommandReply));
    }

    if (error == DS_NoError) {
        *ioResult = DS_IONoError;
        *position = -1;
        error = TRKRequestSend(buffer1, &bufferId2);

        if (error == DS_NoError) {
            buffer2 = TRKGetBuffer(bufferId2);

            if (buffer2 != NULL) {
                *ioResult = ((CommandReply*)buffer2->data)->param.ioResult;
                *position = ((CommandReply*)buffer2->data)->replyPosition;
            }
        }

        TRKReleaseBuffer(bufferId2);
    }

    TRKReleaseBuffer(bufferId1);
    return error;
} // 0x80192698
