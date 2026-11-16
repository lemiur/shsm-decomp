// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/msgbuf.c

#include "TRK/mem_TRK.h"
#include "TRK/msgbuf.h"
#include "TRK/nubinit.h"
#include "TRK/trk_glue.h"

TRKBuffer gTRKMsgBufs[TRK_MSGBUF_COUNT];

/* Marks every message buffer free. */
DSError TRKInitializeMessageBuffers(void) {
    int i;
    for (i = 0; i < TRK_MSGBUF_COUNT; i++) {
        gTRKMsgBufs[i].inUse = FALSE;
    }

    return DS_NoError;
} // 0x80192B64

/* Claims the first free message buffer, returning its ID and address. */
DSError TRKGetFreeBuffer(MessageBufferID* msgID, TRKBuffer** outMsg) {
    DSError error = DS_NoMessageBufferAvailable;
    int i;
    TRKBuffer* buf;

    *outMsg = NULL;

    for (i = 0; i < TRK_MSGBUF_COUNT; i++) {
        buf = TRKGetBuffer(i);

        if (!buf->inUse) {
            TRKResetBuffer(buf, TRUE);
            buf->inUse = TRUE;
            error = DS_NoError;
            *outMsg = buf;
            *msgID = i;
            i = TRK_MSGBUF_COUNT;
        }
    }

    if (error == DS_NoMessageBufferAvailable) {
        OSReport("MetroTRK - ERROR : No buffer available\n");
    }

    return error;
} // 0x80192B84

/* Returns the buffer with the given ID, or NULL when the ID is out of range. */
void* TRKGetBuffer(MessageBufferID id) {
    TRKBuffer* buf = NULL;
    if (id >= 0 && id < TRK_MSGBUF_COUNT) {
        buf = &gTRKMsgBufs[id];
    }

    return buf;
} // 0x80192C20

/* Frees the buffer with the given ID. */
void TRKReleaseBuffer(MessageBufferID id) {
    TRKBuffer* msg;
    if (id != TRK_NO_BUFFER && id >= 0 && id < TRK_MSGBUF_COUNT) {
        msg = &gTRKMsgBufs[id];
        msg->inUse = FALSE;
    }
} // 0x80192C44

/* Empties the buffer. The packet bytes are cleared unless keepData is set. */
void TRKResetBuffer(TRKBuffer* msg, BOOL keepData) {
    msg->length = 0;
    msg->position = 0;

    if (!keepData) {
        TRK_memset(msg->data, 0, TRK_MSGBUF_SIZE);
    }
} // 0x80192C6C

/* Moves the write position to pos, growing the valid length to cover it. */
DSError TRKSetBufferPosition(TRKBuffer* msg, u32 pos) {
    DSError error = DS_NoError;

    if (pos > TRK_MSGBUF_SIZE) {
        error = DS_MessageBufferOverflow;
    } else {
        msg->position = pos;
        if (pos > msg->length) {
            msg->length = pos;
        }
    }

    return error;
} // 0x80192C94

/* Appends length bytes at the write position, clamping to the space left. */
DSError TRKAppendBuffer(TRKBuffer* msg, const void* data, size_t length) {
    DSError error = DS_NoError;
    u32 bytesLeft;

    if (length == 0) {
        return DS_NoError;
    }

    bytesLeft = TRK_MSGBUF_SIZE - msg->position;

    if (bytesLeft < length) {
        error = DS_MessageBufferOverflow;
        length = bytesLeft;
    }

    if (length == 1) {
        msg->data[msg->position] = ((u8*)data)[0];
    } else {
        TRK_memcpy(msg->data + msg->position, data, length);
    }

    msg->position += length;
    msg->length = msg->position;

    return error;
} // 0x80192CC4

/* Reads length bytes from the read position, clamping to the bytes left. */
DSError TRKReadBuffer(TRKBuffer* msg, void* data, size_t length) {
    DSError error = DS_NoError;
    unsigned int bytesLeft;

    if (length == 0) {
        return DS_NoError;
    }

    bytesLeft = msg->length - msg->position;

    if (length > bytesLeft) {
        error = DS_MessageBufferReadError;
        length = bytesLeft;
    }

    TRK_memcpy(data, msg->data + msg->position, length);
    msg->position += length;
    return error;
} // 0x80192D68

/* Appends one byte. No out-of-line copy in retail: inlined into TRKAppendBuffer_ui8. */
DSError TRKAppendBuffer1_ui8(TRKBuffer* buffer, const u8 data) {
    if (buffer->position >= TRK_MSGBUF_SIZE) {
        return DS_MessageBufferOverflow;
    }

    buffer->data[buffer->position++] = data;
    buffer->length++;
    return DS_NoError;
}

/* Appends a 32-bit value in big-endian order, swapping it on a little-endian target. */
DSError TRKAppendBuffer1_ui32(TRKBuffer* buffer, const u32 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;

        bigEndianData[0] = byteData[3];
        bigEndianData[1] = byteData[2];
        bigEndianData[2] = byteData[1];
        bigEndianData[3] = byteData[0];
    }

    return TRKAppendBuffer(buffer, (const void*)bigEndianData, sizeof(data));
} // 0x80192DF8

/* Appends a 64-bit value in big-endian order, swapping it on a little-endian target. */
DSError TRKAppendBuffer1_ui64(TRKBuffer* buffer, const u64 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];
    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;

        bigEndianData[0] = byteData[7];
        bigEndianData[1] = byteData[6];
        bigEndianData[2] = byteData[5];
        bigEndianData[3] = byteData[4];
        bigEndianData[4] = byteData[3];
        bigEndianData[5] = byteData[2];
        bigEndianData[6] = byteData[1];
        bigEndianData[7] = byteData[0];
    }

    return TRKAppendBuffer(buffer, (const void*)bigEndianData, sizeof(data));
} // 0x80192EC8

/* Appends count bytes, stopping at the first error. */
DSError TRKAppendBuffer_ui8(TRKBuffer* buffer, const u8* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKAppendBuffer1_ui8(buffer, data[i]);
    }

    return err;
} // 0x80192FBC

/* Appends count 32-bit values, stopping at the first error. */
DSError TRKAppendBuffer_ui32(TRKBuffer* buffer, const u32* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKAppendBuffer1_ui32(buffer, data[i]);
    }

    return err;
} // 0x80193020

/* Reads one byte. No out-of-line copy in retail: inlined into TRKReadBuffer_ui8. */
DSError TRKReadBuffer1_ui8(TRKBuffer* buffer, u8* data) {
    return TRKReadBuffer(buffer, (void*)data, 1);
}

/* Reads a big-endian 32-bit value, swapping it on a little-endian target. No out-of-line copy in
 * retail: inlined into TRKReadBuffer_ui32. */
DSError TRKReadBuffer1_ui32(TRKBuffer* buffer, u32* data) {
    DSError err;

    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, (void*)bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == DS_NoError) {
        byteData = (u8*)data;

        byteData[0] = bigEndianData[3];
        byteData[1] = bigEndianData[2];
        byteData[2] = bigEndianData[1];
        byteData[3] = bigEndianData[0];
    }

    return err;
}

/* Reads a big-endian 64-bit value, swapping it on a little-endian target. */
DSError TRKReadBuffer1_ui64(TRKBuffer* buffer, u64* data) {
    DSError err;

    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, (void*)bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == 0) {
        byteData = (u8*)data;

        byteData[0] = bigEndianData[7];
        byteData[1] = bigEndianData[6];
        byteData[2] = bigEndianData[5];
        byteData[3] = bigEndianData[4];
        byteData[4] = bigEndianData[3];
        byteData[5] = bigEndianData[2];
        byteData[6] = bigEndianData[1];
        byteData[7] = bigEndianData[0];
    }

    return err;
} // 0x80193110

/* Reads count bytes, stopping at the first error. */
DSError TRKReadBuffer_ui8(TRKBuffer* buffer, u8* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKReadBuffer1_ui8(buffer, &(data[i]));
    }

    return err;
} // 0x801931F0

/* Reads count 32-bit values, stopping at the first error. */
DSError TRKReadBuffer_ui32(TRKBuffer* buffer, u32* data, int count) {
    DSError err;
    s32 i;

    for (i = 0, err = DS_NoError; err == DS_NoError && i < count; i++) {
        err = TRKReadBuffer1_ui32(buffer, &(data[i]));
    }

    return err;
} // 0x80193288
