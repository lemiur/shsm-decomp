#ifndef TRK_NUBEVENT_H
#define TRK_NUBEVENT_H

#include <revolution/types.h>
#include "TRK/dserror.h"
#include "TRK/msgbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kinds of event the nub main loop dispatches. */
typedef enum NubEventType {
    NUBEVENT_Null = 0,
    NUBEVENT_Shutdown = 1,
    NUBEVENT_Request = 2,
    NUBEVENT_Breakpoint = 3,
    NUBEVENT_Exception = 4,
    NUBEVENT_Support = 5
} NubEventType;

typedef u32 NubEventID;

typedef struct TRKEvent {
    NubEventType eventType;
    NubEventID eventID;
    MessageBufferID msgBufID;
} TRKEvent;

#define TRK_EVENT_QUEUE_LENGTH 2

typedef struct TRKEventQueue {
    int count;
    int next;
    TRKEvent events[TRK_EVENT_QUEUE_LENGTH];
    NubEventID eventID;
} TRKEventQueue;

extern TRKEventQueue gTRKEventQueue;

DSError TRKInitializeEventQueue(void);
BOOL TRKGetNextEvent(TRKEvent* event);
DSError TRKPostEvent(TRKEvent* event);
void TRKConstructEvent(TRKEvent* event, NubEventType eventType);
void TRKDestructEvent(TRKEvent* event);

#ifdef __cplusplus
}
#endif

#endif /* TRK_NUBEVENT_H */
