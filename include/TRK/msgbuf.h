// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/msgbuf.c

#ifndef TRK_MSGBUF_H
#define TRK_MSGBUF_H

#include <stddef.h>
#include <revolution/types.h>
#include "TRK/dserror.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Index of a message buffer. TRK_NO_BUFFER marks an event that holds none. */
typedef int MessageBufferID;
#define TRK_NO_BUFFER (-1)

/* Message buffer: state, valid length, write position, then the packet bytes. */
typedef struct TRKBuffer TRKBuffer;

#define TRK_MSGBUF_COUNT 3
#define TRK_MSGBUF_SIZE 0x880

struct TRKBuffer {
    int inUse;                  /* 0x00 */
    u32 length;                 /* 0x04 */
    u32 position;               /* 0x08 */
    u8 data[TRK_MSGBUF_SIZE];   /* 0x0C */
};

extern TRKBuffer gTRKMsgBufs[TRK_MSGBUF_COUNT];

DSError TRKInitializeMessageBuffers(void);
DSError TRKGetFreeBuffer(MessageBufferID* msgID, TRKBuffer** outMsg);
void* TRKGetBuffer(MessageBufferID id);
void TRKReleaseBuffer(MessageBufferID id);
void TRKResetBuffer(TRKBuffer* msg, BOOL keepData);
DSError TRKSetBufferPosition(TRKBuffer* msg, u32 pos);
DSError TRKAppendBuffer(TRKBuffer* msg, const void* data, size_t length);
DSError TRKReadBuffer(TRKBuffer* msg, void* data, size_t length);

DSError TRKAppendBuffer1_ui8(TRKBuffer* buffer, const u8 data);
DSError TRKAppendBuffer1_ui32(TRKBuffer* buffer, const u32 data);
DSError TRKAppendBuffer1_ui64(TRKBuffer* buffer, const u64 data);
DSError TRKAppendBuffer_ui8(TRKBuffer* buffer, const u8* data, int count);
DSError TRKAppendBuffer_ui32(TRKBuffer* buffer, const u32* data, int count);

DSError TRKReadBuffer1_ui8(TRKBuffer* buffer, u8* data);
DSError TRKReadBuffer1_ui32(TRKBuffer* buffer, u32* data);
DSError TRKReadBuffer1_ui64(TRKBuffer* buffer, u64* data);
DSError TRKReadBuffer_ui8(TRKBuffer* buffer, u8* data, int count);
DSError TRKReadBuffer_ui32(TRKBuffer* buffer, u32* data, int count);

#ifdef __cplusplus
}
#endif

#endif /* TRK_MSGBUF_H */
