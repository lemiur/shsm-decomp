#include <revolution/os.h>

/* Appends mutex to the queue of mutexes a thread holds. */
#define PushTail(queue, mutex, link)                                                               \
    do {                                                                                           \
        OSMutex* __prev;                                                                           \
                                                                                                   \
        __prev = (queue)->tail;                                                                    \
        if (__prev == NULL) {                                                                      \
            (queue)->head = (mutex);                                                               \
        } else {                                                                                   \
            __prev->link.next = (mutex);                                                           \
        }                                                                                          \
        (mutex)->link.prev = __prev;                                                               \
        (mutex)->link.next = NULL;                                                                 \
        (queue)->tail = (mutex);                                                                   \
    } while (0)

/* Removes the first mutex of a thread's queue into mutex. */
#define PopHead(queue, mutex, link)                                                                \
    do {                                                                                           \
        OSMutex* __next;                                                                           \
                                                                                                   \
        (mutex) = (queue)->head;                                                                   \
        __next = (mutex)->link.next;                                                               \
        if (__next == NULL) {                                                                      \
            (queue)->tail = NULL;                                                                  \
        } else {                                                                                   \
            __next->link.prev = NULL;                                                              \
        }                                                                                          \
        (queue)->head = __next;                                                                    \
    } while (0)

/* Unlinks mutex from a thread's queue. */
#define PopItem(queue, mutex, link)                                                                \
    do {                                                                                           \
        OSMutex* __next;                                                                           \
        OSMutex* __prev;                                                                           \
                                                                                                   \
        __next = (mutex)->link.next;                                                               \
        __prev = (mutex)->link.prev;                                                               \
                                                                                                   \
        if (__next == NULL) {                                                                      \
            (queue)->tail = __prev;                                                                \
        } else {                                                                                   \
            __next->link.prev = __prev;                                                            \
        }                                                                                          \
                                                                                                   \
        if (__prev == NULL) {                                                                      \
            (queue)->head = __next;                                                                \
        } else {                                                                                   \
            __prev->link.next = __next;                                                            \
        }                                                                                          \
    } while (0)

/*
 * OSInitMutex
 *
 * Makes mutex unlocked, with no waiting threads.
 */
void OSInitMutex(OSMutex* mutex) {
    OSInitThreadQueue(&mutex->queue);
    mutex->thread = NULL;
    mutex->count = 0;
} // 0x80218750

/*
 * OSLockMutex
 *
 * Locks mutex for the current thread, recursively if it already owns it.
 * Waits while another thread owns it, promoting the owner to the current
 * thread's priority.
 */
void OSLockMutex(OSMutex* mutex) {
    BOOL enabled;
    OSThread* currentThread;
    OSThread* ownerThread;

    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();

    while (TRUE) {
        ownerThread = mutex->thread;
        if (ownerThread == NULL) {
            mutex->thread = currentThread;
            mutex->count++;
            PushTail(&currentThread->queueMutex, mutex, link);
            break;
        } else if (ownerThread == currentThread) {
            mutex->count++;
            break;
        } else {
            currentThread->mutex = mutex;
            __OSPromoteThread(mutex->thread, currentThread->priority);
            OSSleepThread(&mutex->queue);
            currentThread->mutex = NULL;
        }
    }

    OSRestoreInterrupts(enabled);
} // 0x80218790

/*
 * OSUnlockMutex
 *
 * Drops one level of the current thread's lock on mutex. The last unlock
 * releases it, restores the thread's priority and wakes the waiters.
 */
void OSUnlockMutex(OSMutex* mutex) {
    BOOL enabled;
    OSThread* currentThread;

    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();

    if (mutex->thread == currentThread && --mutex->count == 0) {
        PopItem(&currentThread->queueMutex, mutex, link);
        mutex->thread = NULL;
        if (currentThread->priority < currentThread->base) {
            currentThread->priority = __OSGetEffectivePriority(currentThread);
        }

        OSWakeupThread(&mutex->queue);
    }

    OSRestoreInterrupts(enabled);
} // 0x80218870

/*
 * __OSUnlockAllMutex
 *
 * Releases every mutex thread holds, for a thread that exits.
 */
void __OSUnlockAllMutex(OSThread* thread) {
    OSMutex* mutex;

    while (thread->queueMutex.head) {
        PopHead(&thread->queueMutex, mutex, link);
        mutex->count = 0;
        mutex->thread = NULL;
        OSWakeupThread(&mutex->queue);
    }
} // 0x80218940

/*
 * __OSCheckMutex
 *
 * Checks the consistency of mutex: the links of its wait queue, the state
 * and priority order of the waiters, and the lock count.
 */
static BOOL __OSCheckMutex(OSMutex* mutex) {
    OSThread* thread;
    OSThreadQueue* queue;
    OSPriority priority;

    priority = 0;
    queue = &mutex->queue;
    if (!(queue->head == NULL || queue->head->link.prev == NULL)) {
        return FALSE;
    }
    if (!(queue->tail == NULL || queue->tail->link.next == NULL)) {
        return FALSE;
    }
    for (thread = queue->head; thread; thread = thread->link.next) {
        if (!(thread->link.next == NULL || thread == thread->link.next->link.prev)) {
            return FALSE;
        }
        if (!(thread->link.prev == NULL || thread == thread->link.prev->link.next)) {
            return FALSE;
        }
        if (thread->state != OS_THREAD_STATE_WAITING) {
            return FALSE;
        }
        if (thread->priority < priority) {
            return FALSE;
        }
        priority = thread->priority;
    }

    if (mutex->thread) {
        if (mutex->count <= 0) {
            return FALSE;
        }
    } else {
        if (0 != mutex->count) {
            return FALSE;
        }
    }

    return TRUE;
} // 0x802189B0

/*
 * __OSCheckDeadLock
 *
 * Returns TRUE if the chain of owners of the mutexes thread waits for leads
 * back to thread.
 */
BOOL __OSCheckDeadLock(OSThread* thread) {
    OSMutex* mutex;

    mutex = thread->mutex;
    while (mutex && mutex->thread) {
        if (mutex->thread == thread) {
            return TRUE;
        }
        mutex = mutex->thread->mutex;
    }

    return FALSE;
} // 0x80218AB0

/*
 * __OSCheckMutexes
 *
 * Checks that thread owns every mutex in its queue and that each is
 * consistent.
 */
BOOL __OSCheckMutexes(OSThread* thread) {
    OSMutex* mutex;

    for (mutex = thread->queueMutex.head; mutex; mutex = mutex->link.next) {
        if (mutex->thread != thread) {
            return FALSE;
        }
        if (!__OSCheckMutex(mutex)) {
            return FALSE;
        }
    }

    return TRUE;
} // 0x80218AF0
