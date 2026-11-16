// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/nubevent.c

#include "TRK/mem_TRK.h"
#include "TRK/msgbuf.h"
#include "TRK/nubevent.h"
#include "TRK/trk_glue.h"

/* Event IDs are handed out from this value upward; the counter restarts here when it wraps. */
#define TRK_FIRST_EVENT_ID 0x100

TRKEventQueue gTRKEventQueue;

/* Empties the event queue and restarts event numbering. */
DSError TRKInitializeEventQueue(void) {
    gTRKEventQueue.count = 0;
    gTRKEventQueue.next = 0;
    gTRKEventQueue.eventID = TRK_FIRST_EVENT_ID;
    return DS_NoError;
} // 0x80191D30

/* Copies the oldest queued event into *event and removes it from the queue. */
BOOL TRKGetNextEvent(TRKEvent* event) {
    BOOL status = FALSE;

    if (gTRKEventQueue.count > 0) {
        TRK_memcpy(event, &gTRKEventQueue.events[gTRKEventQueue.next], sizeof(TRKEvent));
        gTRKEventQueue.count--;
        if (++gTRKEventQueue.next == TRK_EVENT_QUEUE_LENGTH) {
            gTRKEventQueue.next = 0;
        }
        status = TRUE;
    }
    return status;
} // 0x80191D54

/* Appends a copy of *event with a fresh event ID. Reports DS_EventQueueFull when the queue is full. */
DSError TRKPostEvent(TRKEvent* event) {
    DSError ret = DS_NoError;
    int nextEventID;

    if (gTRKEventQueue.count == TRK_EVENT_QUEUE_LENGTH) {
        ret = DS_EventQueueFull;
        OSReport("MetroTRK - Event Queue full\n");
    } else {
        nextEventID = (gTRKEventQueue.next + gTRKEventQueue.count) % TRK_EVENT_QUEUE_LENGTH;
        TRK_memcpy(&gTRKEventQueue.events[nextEventID], event, sizeof(TRKEvent));
        gTRKEventQueue.events[nextEventID].eventID = gTRKEventQueue.eventID;

        if (++gTRKEventQueue.eventID < TRK_FIRST_EVENT_ID) {
            gTRKEventQueue.eventID = TRK_FIRST_EVENT_ID;
        }

        gTRKEventQueue.count++;
    }
    return ret;
} // 0x80191DE0

/* Fills in a new event of the given type with no event ID and no message buffer. */
void TRKConstructEvent(TRKEvent* event, NubEventType eventType) {
    event->eventType = eventType;
    event->eventID = 0;
    event->msgBufID = TRK_NO_BUFFER;
} // 0x80191EAC

/* Releases the message buffer the event holds. */
void TRKDestructEvent(TRKEvent* event) {
    TRKReleaseBuffer(event->msgBufID);
} // 0x80191EC4
