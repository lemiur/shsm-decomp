// Reference: zeldaret/tp libs/revolution/src/dvd/dvdqueue.c
//
// The debug-only ASSERTLINE check compiles to nothing in this release build and is left out.
// __DVDIsBlockInWaitingQueue and DVDDumpWaitingQueue have no retail address (nothing in the game calls them);
// the linker dead-strips them, so they stay here in reference order and are not checked against retail bytes.
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>

/* One list head per priority: next and prev share the layout of the first two DVDCommandBlock fields. */
static struct {
    /* 0x00 */ DVDCommandBlock* next;
    /* 0x04 */ DVDCommandBlock* prev;
} WaitingQueue[4];

// prototypes
static DVDCommandBlock* PopWaitingQueuePrio(s32 prio);

/*
 * __DVDClearWaitingQueue
 *
 * Empties every priority queue: each head points back at itself.
 */
void __DVDClearWaitingQueue(void) {
    u32 i;

    for (i = 0; i < 4; i++) {
        DVDCommandBlock* q = (DVDCommandBlock*)&WaitingQueue[i].next;
        q->next = q;
        q->prev = q;
    }
} // 0x80224710

/*
 * __DVDPushWaitingQueue
 *
 * Appends a command block to the tail of its priority queue.
 */
int __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block) {
    BOOL enabled = OSDisableInterrupts();
    DVDCommandBlock* q = (DVDCommandBlock*)&WaitingQueue[prio];

    q->prev->next = block;
    block->prev = q->prev;
    block->next = q;
    q->prev = block;
    OSRestoreInterrupts(enabled);
    return 1;
} // 0x80224750

/*
 * PopWaitingQueuePrio
 *
 * Removes the head command block of one priority queue.
 */
static DVDCommandBlock* PopWaitingQueuePrio(s32 prio) {
    DVDCommandBlock* tmp;
    BOOL enabled;
    DVDCommandBlock* q;

    enabled = OSDisableInterrupts();
    q = (DVDCommandBlock*)&WaitingQueue[prio];

    tmp = q->next;
    q->next = tmp->next;
    tmp->next->prev = q;
    OSRestoreInterrupts(enabled);
    tmp->next = 0;
    tmp->prev = 0;
    return tmp;
}

/*
 * __DVDPopWaitingQueue
 *
 * Removes the head of the highest-priority non-empty queue, or returns NULL.
 */
DVDCommandBlock* __DVDPopWaitingQueue(void) {
    u32 i;
    BOOL enabled;
    DVDCommandBlock* q;

    enabled = OSDisableInterrupts();
    for (i = 0; i < 4; i++) {
        q = (DVDCommandBlock*)&WaitingQueue[i];
        if (q->next != q) {
            OSRestoreInterrupts(enabled);
            return PopWaitingQueuePrio(i);
        }
    }

    OSRestoreInterrupts(enabled);
    return NULL;
} // 0x802247C0

/*
 * __DVDCheckWaitingQueue
 *
 * Returns 1 when any priority queue has a waiting command block, else 0.
 */
int __DVDCheckWaitingQueue(void) {
    u32 i;
    BOOL enabled;
    DVDCommandBlock* q;

    enabled = OSDisableInterrupts();
    for (i = 0; i < 4; i++) {
        q = (DVDCommandBlock*)&WaitingQueue[i];
        if (q->next != q) {
            OSRestoreInterrupts(enabled);
            return 1;
        }
    }

    OSRestoreInterrupts(enabled);
    return 0;
} // 0x80224860

/*
 * __DVDGetNextWaitingQueue
 *
 * Returns the head of the highest-priority non-empty queue without removing it,
 * or NULL when every queue is empty.
 */
DVDCommandBlock* __DVDGetNextWaitingQueue(void) {
    u32 i;
    BOOL enabled;
    DVDCommandBlock* q, *tmp;

    enabled = OSDisableInterrupts();

    for (i = 0; i < 4; i++) {
        q = (DVDCommandBlock*)&(WaitingQueue[i]);

        if (q->next != q) {
            tmp = q->next;
            OSRestoreInterrupts(enabled);
            return tmp;
        }
    }

    OSRestoreInterrupts(enabled);
    return NULL;
} // 0x802248C0

/*
 * __DVDDequeueWaitingQueue
 *
 * Unlinks a command block from whichever queue holds it. Returns 0 when the
 * block is not linked.
 */
int __DVDDequeueWaitingQueue(DVDCommandBlock* block) {
    BOOL enabled;
    DVDCommandBlock* prev;
    DVDCommandBlock* next;

    enabled = OSDisableInterrupts();
    prev = block->prev;
    next = block->next;
    if (prev == NULL || next == NULL) {
        OSRestoreInterrupts(enabled);
        return 0;
    }
    prev->next = next;
    next->prev = prev;
    OSRestoreInterrupts(enabled);
    return 1;
} // 0x80224930

/*
 * __DVDIsBlockInWaitingQueue
 *
 * Returns 1 when the block is queued on any priority queue, else 0.
 */
int __DVDIsBlockInWaitingQueue(DVDCommandBlock* block) {
    u32 i;
    DVDCommandBlock* start;
    DVDCommandBlock* q;

    for (i = 0; i < 4; i++) {
        start = (DVDCommandBlock*)&WaitingQueue[i];
        if (start->next == start) {
            continue;
        }

        for (q = start->next; q != start; q = q->next) {
            if (q == block) {
                return 1;
            }
        }
    }

    return 0;
}

static char* CommandNames[16] = {
    "",
    "READ",
    "SEEK",
    "CHANGE_DISK",
    "BSREAD",
    "READID",
    "INITSTREAM",
    "CANCELSTREAM",
    "STOP_STREAM_AT_END",
    "REQUEST_AUDIO_ERROR",
    "REQUEST_PLAY_ADDR",
    "REQUEST_START_ADDR",
    "REQUEST_LENGTH",
    "AUDIO_BUFFER_CONFIG",
    "INQUIRY",
    "BS_CHANGE_DISK",
};

/*
 * DVDDumpWaitingQueue
 *
 * Prints every priority queue and the commands waiting on it.
 */
void DVDDumpWaitingQueue(void) {
    u32 i;
    DVDCommandBlock* start;
    DVDCommandBlock* q;

    OSReport("==== DVD Waiting Queue Status ====\n");
    for (i = 0; i < 4; i++) {
        OSReport("< Queue #%d > ", i);
        start = (DVDCommandBlock*)&WaitingQueue[i];
        if (start->next == start) {
            OSReport("None\n");
        } else {
            OSReport("\n");
            for (q = start->next; q != start; q = q->next) {
                OSReport("0x%08x: Command: %s ", q, CommandNames[q->command]);
                if (q->command == 1) {
                    OSReport("Disk offset: %d, Length: %d, Addr: 0x%08x\n", q->offset, q->length, q->addr);
                } else {
                    OSReport("\n");
                }
            }
        }
    }
}
