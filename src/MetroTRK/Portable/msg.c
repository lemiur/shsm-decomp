// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/msg.c

#include "TRK/msg.h"
#include "TRK/support.h"
#include "TRK/trk_glue.h"
#include "TRK/uart.h"

/* Sequence number stamped into each outgoing message; 0 is skipped. */
static u16 gTRKMsgSeq;

/* Stamps the message with the next sequence number and writes it to the UART. */
DSError TRKMessageSend(TRKBuffer* msg) {
    UARTError err;

    if (gTRKMsgSeq == 0) {
        gTRKMsgSeq = 1;
    }
    ((CommandReply*)msg->data)->sequence = gTRKMsgSeq++;

    err = TRKWriteUARTN(msg->data, msg->length);
    if (err != kUARTNoError) {
        OSReport("MetroTRK - TRK_WriteUARTN returned %ld\n", err);
    }
    return DS_NoError;
} // 0x80192B00
