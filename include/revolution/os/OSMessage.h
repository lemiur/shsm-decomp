#ifndef REVOLUTION_OS_OSMESSAGE_H
#define REVOLUTION_OS_OSMESSAGE_H

#include <revolution/types.h>
#include <revolution/os/OSThread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* OSMessage;

/* A fixed-size ring of messages, with the threads waiting to send and to receive. */
typedef struct OSMessageQueue {
    OSThreadQueue queueSend;    // 0x00: threads waiting for a free slot
    OSThreadQueue queueReceive; // 0x08: threads waiting for a message
    OSMessage* msgArray;        // 0x10
    s32 msgCount;               // 0x14: capacity of msgArray
    s32 firstIndex;             // 0x18: oldest message
    s32 usedCount;              // 0x1C
} OSMessageQueue;

/* OSSendMessage/OSReceiveMessage/OSJamMessage flags. */
#define OS_MESSAGE_NOBLOCK 0
#define OS_MESSAGE_BLOCK   1

void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount);
BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags);
BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags);
BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags);

#ifdef __cplusplus
}
#endif

#endif
