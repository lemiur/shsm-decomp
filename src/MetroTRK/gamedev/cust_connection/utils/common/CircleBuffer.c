// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/gamedev/cust_connection/utils/common/CircleBuffer.c

#include <string.h>

#include "TRK/CircleBuffer.h"
#include "TRK/MWCriticalSection_gc.h"

/* Number of bytes waiting to be read. */
u32 CBGetBytesAvailableForRead(CircleBuffer* cb) {
    return cb->mBytesToRead;
} // 0x80190EC0

/* Points cb at buf, which holds size bytes, and empties the ring. */
void CircleBufferInitialize(CircleBuffer* cb, u8* buf, s32 size) {
    cb->start_ptr = buf;
    cb->size = size;
    cb->read_ptr = cb->start_ptr;
    cb->write_ptr = cb->start_ptr;
    cb->mBytesToRead = 0;
    cb->mBytesToWrite = cb->size;
    MWInitializeCriticalSection(&cb->mCriticalSection);
} // 0x80190EC8

/* Copies size bytes from buf into the ring, wrapping at the end. Returns -1 without copying when there is
 * not enough free space, 0 otherwise. */
int CircleBufferWriteBytes(CircleBuffer* cb, u8* buf, u32 size) {
    int availSize;

    if (size > cb->mBytesToWrite) {
        return -1;
    }
    MWEnterCriticalSection(&cb->mCriticalSection);
    availSize = cb->size - (cb->write_ptr - cb->start_ptr);
    if (availSize >= size) {
        memcpy(cb->write_ptr, buf, size);
        cb->write_ptr += size;
    } else {
        memcpy(cb->write_ptr, buf, availSize);
        memcpy(cb->start_ptr, buf + availSize, size - availSize);
        cb->write_ptr = cb->start_ptr + size - availSize;
    }

    if (cb->size == (cb->write_ptr - cb->start_ptr)) {
        cb->write_ptr = cb->start_ptr;
    }

    cb->mBytesToWrite -= size;
    cb->mBytesToRead += size;
    MWExitCriticalSection(&cb->mCriticalSection);
    return 0;
} // 0x80190EEC

/* Copies size bytes from the ring into buf, wrapping at the end. Returns -1 without copying when fewer bytes
 * are queued than requested, 0 otherwise. */
int CircleBufferReadBytes(CircleBuffer* cb, u8* buf, u32 size) {
    int availSize;

    if (size > cb->mBytesToRead) {
        return -1;
    }
    MWEnterCriticalSection(&cb->mCriticalSection);
    availSize = cb->size - (cb->read_ptr - cb->start_ptr);
    if (size < availSize) {
        memcpy(buf, cb->read_ptr, size);
        cb->read_ptr += size;
    } else {
        memcpy(buf, cb->read_ptr, availSize);
        memcpy(buf + availSize, cb->start_ptr, size - availSize);
        cb->read_ptr = cb->start_ptr + size - availSize;
    }

    if (cb->size == (cb->read_ptr - cb->start_ptr)) {
        cb->read_ptr = cb->start_ptr;
    }

    cb->mBytesToWrite += size;
    cb->mBytesToRead -= size;
    MWExitCriticalSection(&cb->mCriticalSection);
    return 0;
} // 0x80190FF4
