// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/support.c

#ifndef TRK_SUPPORT_H
#define TRK_SUPPORT_H

#include <stddef.h>
#include <revolution/types.h>
#include "TRK/dserror.h"
#include "TRK/dsmsg.h"
#include "TRK/msgbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

DSError TRKSuppAccessFile(u32 file_handle, u8* data, size_t* count, DSIOResult* io_result, BOOL need_reply, BOOL read);
DSError TRKRequestSend(TRKBuffer* msgBuf, MessageBufferID* bufferId);
DSError HandleOpenFileSupportRequest(const char* path, u8 replyError, u32* fileHandle, DSIOResult* ioResult);
DSError HandleCloseFileSupportRequest(int replyError, DSIOResult* ioResult);
DSError HandlePositionFileSupportRequest(DSReplyError replyErr, u32* position, u8 seekMode, DSIOResult* ioResult);

#ifdef __cplusplus
}
#endif

#endif /* TRK_SUPPORT_H */
