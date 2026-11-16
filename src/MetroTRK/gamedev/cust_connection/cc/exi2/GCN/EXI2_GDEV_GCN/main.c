// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/gamedev/cust_connection/cc/exi2/GCN/EXI2_GDEV_GCN/main.c

#include "TRK/CircleBuffer.h"
#include "TRK/GDEV_Stubs.h"
#include "amc/AmcExi2Comm.h"

/* Size of the receive buffer behind gRecvCB. */
#define GDEV_BUF_SIZE 0x500

/* Packets pulled from the DB layer wait in gRecvCB until gdev_cc_read takes them. */
static CircleBuffer gRecvCB;
static u8 gRecvBuf[GDEV_BUF_SIZE];

/* Set by gdev_cc_open. Read, write and peek refuse to run before that. */
static BOOL gIsInitialized;

/* Starts the DB comm layer and points the receive ring at gRecvBuf. Returns 0. */
int gdev_cc_initialize(void* inputPendingPtrRef, EXICallback monitorCallback) {
    DBInitComm(inputPendingPtrRef, monitorCallback);
    CircleBufferInitialize(&gRecvCB, gRecvBuf, GDEV_BUF_SIZE);
    return 0;
} // 0x80190C0C

/* Nothing to release. Returns 0. */
int gdev_cc_shutdown(void) {
    return 0;
} // 0x80190C48

/* Marks the connection open. Returns -10005 when it is already open. */
int gdev_cc_open(void) {
    if (gIsInitialized != 0) {
        return -10005; /* already open */
    }

    gIsInitialized = TRUE;
    return 0;
} // 0x80190C50

/* Nothing to do on close. Returns 0. */
int gdev_cc_close(void) {
    return 0;
} // 0x80190C74

/* Fills data with size bytes. Polls the DB layer and queues what it returns until the ring holds enough,
 * then copies the bytes out. Returns 0, the DB read error, or -0x2711 when the connection is not open. */
int gdev_cc_read(u8* data, int size) {
    u8 buff[GDEV_BUF_SIZE];
    u32 retval;
    int poll;

    retval = 0;
    if (!gIsInitialized) {
        return -0x2711; /* not open */
    }

    while (CBGetBytesAvailableForRead(&gRecvCB) < size) {
        retval = 0;
        poll = DBQueryData();
        if (poll != 0) {
            retval = DBRead(buff, size);
            if (retval == 0) {
                CircleBufferWriteBytes(&gRecvCB, buff, poll);
            }
        }
    }

    if (retval == 0) {
        CircleBufferReadBytes(&gRecvCB, data, size);
    }

    return retval;
} // 0x80190C7C

/* Sends length bytes from bytes through the DB layer, retrying from where each write stopped. Returns 0, or
 * -0x2711 when the connection is not open. */
int gdev_cc_write(const u8* bytes, int length) {
    int remaining;
    const u8* cursor;
    int written;

    cursor = bytes;
    remaining = length;
    if (gIsInitialized == FALSE) {
        return -0x2711; /* not open */
    }

    while (remaining > 0) {
        written = DBWrite(cursor, remaining);
        if (written == AMC_EXI_NO_ERROR) {
            break;
        }
        cursor += written;
        remaining -= written;
    }

    return 0;
} // 0x80190D30

/* Closes the DB link. Called before the target resumes. Returns 0. */
int gdev_cc_pre_continue(void) {
    DBClose();
    return 0;
} // 0x80190DA4

/* Opens the DB link again. Called after the target stops. Returns 0. */
int gdev_cc_post_stop(void) {
    DBOpen();
    return 0;
} // 0x80190DC8

/* Moves pending DB data into gRecvCB; the bytes stay queued for gdev_cc_read. Returns the byte count, 0 when
 * nothing is pending, or -0x2719 when the DB read fails. */
int gdev_cc_peek(void) {
    s32 poll;
    u8 buff[GDEV_BUF_SIZE];

    poll = DBQueryData();
    if (poll <= 0) {
        return 0;
    }

    if (DBRead(buff, poll) == 0) {
        CircleBufferWriteBytes(&gRecvCB, buff, poll);
    } else {
        return -0x2719; /* DB read failed */
    }

    return poll;
} // 0x80190DEC

/* Turns on the DB interrupts. Returns 0. */
int gdev_cc_initinterrupts(void) {
    DBInitInterrupts();
    return 0;
} // 0x80190E60
