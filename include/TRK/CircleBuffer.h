// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/utils/common/CircleBuffer.h

#ifndef TRK_CIRCLEBUFFER_H
#define TRK_CIRCLEBUFFER_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Byte ring with a read pointer and a write pointer. mBytesToRead and mBytesToWrite count the filled and free
 * bytes; mCriticalSection holds the interrupt state saved by the write and read paths. */
typedef struct CircleBuffer {
    u8* read_ptr;
    u8* write_ptr;
    u8* start_ptr;
    u32 size;
    s32 mBytesToRead;
    u32 mBytesToWrite;
    u32 mCriticalSection;
} CircleBuffer;

/* Copies size bytes out of the ring into buf. Returns -1 without copying when fewer bytes are queued, 0 otherwise. */
int CircleBufferReadBytes(CircleBuffer* cb, u8* buf, u32 size);
/* Copies size bytes from buf into the ring. Returns -1 without copying when there is not enough free space, 0 otherwise. */
int CircleBufferWriteBytes(CircleBuffer* cb, u8* buf, u32 size);
/* Points cb at buf, which holds size bytes, and empties the ring. */
void CircleBufferInitialize(CircleBuffer* cb, u8* buf, s32 size);
/* Number of bytes waiting to be read. */
u32 CBGetBytesAvailableForRead(CircleBuffer* cb);

#ifdef __cplusplus
}
#endif

#endif /* TRK_CIRCLEBUFFER_H */
