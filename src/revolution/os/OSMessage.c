#include <revolution/os.h>

/*
 * OSInitMessageQueue
 *
 * Makes mq an empty queue over msgArray, which holds msgCount messages.
 */
void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
    OSInitThreadQueue(&mq->queueSend);
    OSInitThreadQueue(&mq->queueReceive);
    mq->msgArray = msgArray;
    mq->msgCount = msgCount;
    mq->firstIndex = 0;
    mq->usedCount = 0;
} // 0x80217CD0

/*
 * OSSendMessage
 *
 * Appends msg to mq. With OS_MESSAGE_BLOCK, waits for a free slot;
 * otherwise returns FALSE when the queue is full.
 */
BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    BOOL enabled;
    s32 lastIndex;

    enabled = OSDisableInterrupts();

    while (mq->msgCount <= mq->usedCount) {
        if (!(flags & OS_MESSAGE_BLOCK)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        } else {
            OSSleepThread(&mq->queueSend);
        }
    }

    lastIndex = (mq->firstIndex + mq->usedCount) % mq->msgCount;
    mq->msgArray[lastIndex] = msg;
    mq->usedCount++;

    OSWakeupThread(&mq->queueReceive);

    OSRestoreInterrupts(enabled);
    return TRUE;
} // 0x80217D30

/*
 * OSReceiveMessage
 *
 * Removes the oldest message of mq into *msg (if msg is not NULL). With
 * OS_MESSAGE_BLOCK, waits for a message; otherwise returns FALSE when the
 * queue is empty.
 */
BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
    BOOL enabled;

    enabled = OSDisableInterrupts();

    while (mq->usedCount == 0) {
        if (!(flags & OS_MESSAGE_BLOCK)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        } else {
            OSSleepThread(&mq->queueReceive);
        }
    }

    if (msg != NULL) {
        *msg = mq->msgArray[mq->firstIndex];
    }
    mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
    mq->usedCount--;

    OSWakeupThread(&mq->queueSend);

    OSRestoreInterrupts(enabled);
    return TRUE;
} // 0x80217E00

/*
 * OSJamMessage
 *
 * Inserts msg at the front of mq, so that it is received next. Blocks or
 * fails on a full queue as OSSendMessage does.
 */
BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    BOOL enabled;

    enabled = OSDisableInterrupts();

    while (mq->msgCount <= mq->usedCount) {
        if (!(flags & OS_MESSAGE_BLOCK)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        } else {
            OSSleepThread(&mq->queueSend);
        }
    }

    mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
    mq->msgArray[mq->firstIndex] = msg;
    mq->usedCount++;

    OSWakeupThread(&mq->queueReceive);

    OSRestoreInterrupts(enabled);
    return TRUE;
} // 0x80217EE0
