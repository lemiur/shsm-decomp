// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/serpoll.c

#include "TRK/msgbuf.h"
#include "TRK/nubevent.h"
#include "TRK/serpoll.h"
#include "TRK/uart.h"

/* Input-pending pointer. The UART layer fills it in during nub start-up, when the nub passes its address. */
void* gTRKInputPendingPtr;

/* Reads one packet from the UART into a new message buffer. Returns the buffer ID, or -1 when no packet is
 * waiting or the packet cannot be read; the buffer is released in that case. */
MessageBufferID TRKTestForPacket(void) {
    u8 payloadBuf[0x880];
    u8 packetBuf[0x40];
    int bufID;
    TRKBuffer* msg;
    MessageBufferID result;

    if (TRKPollUART() <= 0) {
        return -1;
    }

    result = TRKGetFreeBuffer(&bufID, &msg);
    TRKSetBufferPosition(msg, 0);
    if (TRKReadUARTN(packetBuf, 0x40) == kUARTNoError) {
        int readSize;

        TRKAppendBuffer_ui8(msg, packetBuf, 0x40);
        readSize = ((u32*)packetBuf)[0] - 0x40;
        result = bufID;
        if (readSize > 0) {
            if (TRKReadUARTN(payloadBuf, readSize) == kUARTNoError) {
                TRKAppendBuffer_ui8(msg, payloadBuf, ((u32*)packetBuf)[0]);
            } else {
                TRKReleaseBuffer(result);
                result = -1;
            }
        }
    } else {
        /* Original bug: this releases the status TRKGetFreeBuffer returned, not bufID. */
        TRKReleaseBuffer(result);
        result = -1;
    }
    return result;
} // 0x8019201C

/* Polls for a packet and, when one arrived, queues it as a request event. */
void TRKGetInput(void) {
    MessageBufferID id = TRKTestForPacket();

    if (id != -1) {
        TRKProcessInput(id);
    }
} // 0x801920E8

/* Queues a request event for the message buffer that holds an incoming packet. */
void TRKProcessInput(int bufferIdx) {
    TRKEvent event;

    TRKConstructEvent(&event, NUBEVENT_Request);
    event.msgBufID = bufferIdx;
    TRKPostEvent(&event);
} // 0x80192114

/* Brings the serial handler up. This build keeps no state for it. */
DSError TRKInitializeSerialHandler(void) {
    return DS_NoError;
} // 0x80192154

/* Shuts the serial handler down. This build keeps no state for it. */
DSError TRKTerminateSerialHandler(void) {
    return DS_NoError;
} // 0x8019215C
