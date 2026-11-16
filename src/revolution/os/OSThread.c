// Reference: zeldaret/tp libs/revolution/src/os/OSThread.c (rewritten against this tree's headers)
#include <revolution/base.h>
#include <revolution/os.h>
#include <PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h>

/* Appends thread to the tail of queue through its link member. */
#define AddTail(queue, thread, link)                                                               \
    do {                                                                                           \
        OSThread* __prev;                                                                          \
                                                                                                   \
        __prev = (queue)->tail;                                                                    \
        if (__prev == NULL) {                                                                      \
            (queue)->head = (thread);                                                              \
        } else {                                                                                   \
            __prev->link.next = (thread);                                                          \
        }                                                                                          \
        (thread)->link.prev = __prev;                                                              \
        (thread)->link.next = NULL;                                                                \
        (queue)->tail = (thread);                                                                  \
    } while (0)

/* Inserts thread into queue behind every thread of the same or higher priority. */
#define AddPrio(queue, thread, link)                                                               \
    do {                                                                                           \
        OSThread* __prev;                                                                          \
        OSThread* __next;                                                                          \
                                                                                                   \
        for (__next = (queue)->head; __next && __next->priority <= (thread)->priority;            \
             __next = __next->link.next)                                                           \
            ;                                                                                      \
        if (__next == NULL) {                                                                      \
            AddTail(queue, thread, link);                                                          \
        } else {                                                                                   \
            (thread)->link.next = __next;                                                          \
            __prev = __next->link.prev;                                                            \
            __next->link.prev = (thread);                                                          \
            (thread)->link.prev = __prev;                                                          \
            if (__prev == NULL) {                                                                  \
                (queue)->head = (thread);                                                          \
            } else {                                                                               \
                __prev->link.next = (thread);                                                      \
            }                                                                                      \
        }                                                                                          \
    } while (0)

/* Unlinks thread from queue. */
#define RemoveItem(queue, thread, link)                                                            \
    do {                                                                                           \
        OSThread* __next;                                                                          \
        OSThread* __prev;                                                                          \
                                                                                                   \
        __next = (thread)->link.next;                                                              \
        __prev = (thread)->link.prev;                                                              \
        if (__next == NULL) {                                                                      \
            (queue)->tail = __prev;                                                                \
        } else {                                                                                   \
            __next->link.prev = __prev;                                                            \
        }                                                                                          \
        if (__prev == NULL) {                                                                      \
            (queue)->head = __next;                                                                \
        } else {                                                                                   \
            __prev->link.next = __next;                                                            \
        }                                                                                          \
    } while (0)

/* Removes the first thread of queue into thread. */
#define RemoveHead(queue, thread, link)                                                            \
    do {                                                                                           \
        OSThread* __next;                                                                          \
                                                                                                   \
        (thread) = (queue)->head;                                                                  \
        __next = (thread)->link.next;                                                              \
        if (__next == NULL) {                                                                      \
            (queue)->tail = NULL;                                                                  \
        } else {                                                                                   \
            __next->link.prev = NULL;                                                              \
        }                                                                                          \
        (queue)->head = __next;                                                                    \
    } while (0)

#define IsSuspended(suspend) (0 < (suspend))

/* Reports a failed consistency check of OSCheckActiveThreads and halts. */
#define ASSERTREPORT(line, cond)                                                                   \
    if (!(cond)) {                                                                                 \
        OSReport("OSCheckActiveThreads: Failed " #cond " in %d\n", line);                          \
        OSPanic(__FILE__, line, "");                                                               \
    }

static OSThread IdleThread;
static OSContext IdleContext;
static OSThreadQueue RunQueue[OS_PRIORITY_MAX + 1];
static OSThread DefaultThread;

// volatile: SelectThread spins on RunQueueBits with interrupts enabled until a handler readies a thread
static volatile u32 RunQueueBits;
// volatile: set from interrupt handlers (alarms, wakeups) and read by the scheduler
static volatile BOOL RunQueueHint;
// volatile: scheduler lock count, changed under interrupt disable and read by SelectThread
static volatile s32 Reschedule;

static void DefaultSwitchThreadCallback(OSThread* from, OSThread* to);

static OSSwitchThreadCallback SwitchThreadCallback = DefaultSwitchThreadCallback;

/*
 * DefaultSwitchThreadCallback
 *
 * Thread switch callback used while none is installed.
 */
static void DefaultSwitchThreadCallback(OSThread* from, OSThread* to) {
} // 0x8021A1A0

/*
 * OSSetSwitchThreadCallback
 *
 * Installs callback, called on every thread switch; NULL restores the
 * default. Returns the previous callback, or NULL for the default.
 */
OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback) {
    BOOL enabled;
    OSSwitchThreadCallback prev;

    enabled = OSDisableInterrupts();
    prev = SwitchThreadCallback;
    SwitchThreadCallback = callback ? callback : DefaultSwitchThreadCallback;
    OSRestoreInterrupts(enabled);
    return prev == DefaultSwitchThreadCallback ? NULL : prev;
}

/*
 * OSSetCurrentThread
 *
 * Tells the switch callback about the switch and makes thread current.
 */
static void OSSetCurrentThread(OSThread* thread) {
    SwitchThreadCallback(__OSCurrentThread, thread);
    __OSCurrentThread = thread;
}

/*
 * __OSThreadInit
 *
 * Turns the boot context into the default thread and empties the run queues.
 */
void __OSThreadInit(void) {
    OSThread* thread = &DefaultThread;
    OSPriority prio;

    thread->state = OS_THREAD_STATE_RUNNING;
    thread->attr = OS_THREAD_ATTR_DETACH;
    thread->priority = thread->base = 16;
    thread->suspend = 0;
    thread->val = (void*)-1;
    thread->mutex = NULL;
    OSInitThreadQueue(&thread->queueJoin);
    thread->queueMutex.head = thread->queueMutex.tail = NULL;

    __OSFPUContext = &thread->context;
    OSClearContext(&thread->context);
    OSSetCurrentContext(&thread->context);
    thread->stackBase = (u8*)_stack_addr;
    thread->stackEnd = (u32*)_stack_end;
    *thread->stackEnd = OS_THREAD_STACK_MAGIC;
    OSSetCurrentThread(thread);
    OSClearStack(0);

    RunQueueBits = 0;
    RunQueueHint = FALSE;
    for (prio = OS_PRIORITY_MIN; prio <= OS_PRIORITY_MAX; ++prio) {
        OSInitThreadQueue(&RunQueue[prio]);
    }

    OSInitThreadQueue(&__OSActiveThreadQueue);
    AddTail(&__OSActiveThreadQueue, thread, linkActive);
    OSClearContext(&IdleContext);
    Reschedule = 0;
} // 0x8021A1B0

/*
 * OSInitThreadQueue
 *
 * Empties queue.
 */
void OSInitThreadQueue(OSThreadQueue* queue) {
    queue->head = queue->tail = NULL;
} // 0x8021A440

/*
 * OSGetCurrentThread
 *
 * Returns the running thread.
 */
OSThread* OSGetCurrentThread(void) {
    return __OSCurrentThread;
} // 0x8021A450

/*
 * __OSSwitchThread
 *
 * Makes nextThread current and resumes it.
 */
static void __OSSwitchThread(OSThread* nextThread) {
    OSSetCurrentThread(nextThread);
    OSSetCurrentContext(&nextThread->context);
    OSLoadContext(&nextThread->context);
}

/*
 * OSIsThreadSuspended
 *
 * Returns TRUE if thread is suspended.
 */
BOOL OSIsThreadSuspended(OSThread* thread) {
    return thread->suspend > 0 ? TRUE : FALSE;
}

/*
 * OSIsThreadTerminated
 *
 * Returns TRUE if thread has exited or was never created.
 */
BOOL OSIsThreadTerminated(OSThread* thread) {
    return (thread->state == OS_THREAD_STATE_MORIBUND || thread->state == 0) ? TRUE : FALSE;
}

/*
 * __OSIsThreadActive
 *
 * Returns TRUE if thread is in the active thread queue.
 */
static BOOL __OSIsThreadActive(OSThread* thread) {
    OSThread* active;

    if (thread->state == 0) {
        return FALSE;
    }

    for (active = __OSActiveThreadQueue.head; active; active = active->linkActive.next) {
        if (thread == active) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * OSDisableScheduler
 *
 * Locks the scheduler; returns the previous lock count.
 */
s32 OSDisableScheduler(void) {
    BOOL enabled;
    s32 count;

    enabled = OSDisableInterrupts();
    count = Reschedule++;
    OSRestoreInterrupts(enabled);
    return count;
} // 0x8021A460

/*
 * OSEnableScheduler
 *
 * Unlocks the scheduler; returns the previous lock count.
 */
s32 OSEnableScheduler(void) {
    BOOL enabled;
    s32 count;

    enabled = OSDisableInterrupts();
    count = Reschedule--;
    OSRestoreInterrupts(enabled);
    return count;
} // 0x8021A4A0

/*
 * SetRun
 *
 * Queues a ready thread on the run queue of its priority.
 */
static void SetRun(OSThread* thread) {
    ASSERT(!IsSuspended(thread->suspend));
    ASSERT(thread->state == OS_THREAD_STATE_READY);
    ASSERT(OS_PRIORITY_MIN <= thread->priority && thread->priority <= OS_PRIORITY_MAX);

    thread->queue = &RunQueue[thread->priority];
    AddTail(thread->queue, thread, link);
    RunQueueBits |= 1u << (OS_PRIORITY_MAX - thread->priority);
    RunQueueHint = TRUE;
}

/*
 * UnsetRun
 *
 * Takes a ready thread off its run queue.
 */
static void UnsetRun(OSThread* thread) {
    OSThreadQueue* queue;

    ASSERT(thread->state == OS_THREAD_STATE_READY);
    ASSERT(OS_PRIORITY_MIN <= thread->priority && thread->priority <= OS_PRIORITY_MAX);
    ASSERT(thread->queue == &RunQueue[thread->priority]);

    queue = thread->queue;
    RemoveItem(queue, thread, link);
    if (queue->head == NULL) {
        RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - thread->priority));
    }
    thread->queue = NULL;
} // 0x8021A4E0

/*
 * __OSGetEffectivePriority
 *
 * Returns the base priority of thread, raised to that of the first waiter of
 * every mutex it holds.
 */
OSPriority __OSGetEffectivePriority(OSThread* thread) {
    OSPriority priority;
    OSMutex* mutex;
    OSThread* blocked;

    priority = thread->base;
    for (mutex = thread->queueMutex.head; mutex; mutex = mutex->link.next) {
        blocked = mutex->queue.head;
        if (blocked && blocked->priority < priority) {
            priority = blocked->priority;
        }
    }
    return priority;
} // 0x8021A550

/*
 * SetEffectivePriority
 *
 * Moves thread to priority in whatever queue it is on. Returns the owner of
 * the mutex thread waits for, whose priority may change in turn.
 */
static OSThread* SetEffectivePriority(OSThread* thread, OSPriority priority) {
    switch (thread->state) {
    case OS_THREAD_STATE_READY:
        UnsetRun(thread);
        thread->priority = priority;
        SetRun(thread);
        break;
    case OS_THREAD_STATE_WAITING:
        RemoveItem(thread->queue, thread, link);
        thread->priority = priority;
        AddPrio(thread->queue, thread, link);
        if (thread->mutex) {
            return thread->mutex->thread;
        }
        break;
    case OS_THREAD_STATE_RUNNING:
        RunQueueHint = TRUE;
        thread->priority = priority;
        break;
    }
    return NULL;
} // 0x8021A590

/*
 * UpdatePriority
 *
 * Recomputes the effective priority of thread and of the mutex owners it
 * waits behind.
 */
static void UpdatePriority(OSThread* thread) {
    OSPriority priority;

    do {
        if (thread->suspend > 0) {
            break;
        }
        priority = __OSGetEffectivePriority(thread);
        if (thread->priority == priority) {
            break;
        }
        thread = SetEffectivePriority(thread, priority);
    } while (thread);
}

/*
 * __OSPromoteThread
 *
 * Raises thread to priority, passing the raise on along the chain of mutex
 * owners.
 */
void __OSPromoteThread(OSThread* thread, OSPriority priority) {
    do {
        if (thread->suspend > 0 || thread->priority <= priority) {
            break;
        }
        thread = SetEffectivePriority(thread, priority);
    } while (thread);
} // 0x8021A740

/*
 * SelectThread
 *
 * Saves the running thread and switches to the highest-priority ready thread,
 * idling with interrupts enabled while none is ready. Unless yield is set, the
 * running thread keeps the processor when no ready thread outranks it.
 */
static OSThread* SelectThread(BOOL yield) {
    OSContext* currentContext;
    OSThread* currentThread;
    OSThread* nextThread;
    OSPriority priority;
    OSThreadQueue* queue;

    if (Reschedule > 0) {
        return NULL;
    }

    currentContext = OSGetCurrentContext();
    currentThread = OSGetCurrentThread();
    if (currentContext != &currentThread->context) {
        return NULL;
    }

    if (currentThread) {
        if (currentThread->state == OS_THREAD_STATE_RUNNING) {
            if (!yield) {
                priority = __cntlzw(RunQueueBits);
                if (currentThread->priority <= priority) {
                    return NULL;
                }
            }
            currentThread->state = OS_THREAD_STATE_READY;
            SetRun(currentThread);
        }

        if (!(currentThread->context.state & OS_CONTEXT_STATE_EXC) &&
            OSSaveContext(&currentThread->context)) {
            return NULL;
        }
    }

    if (RunQueueBits == 0) {
        OSSetCurrentThread(NULL);
        OSSetCurrentContext(&IdleContext);
        do {
            OSEnableInterrupts();
            while (RunQueueBits == 0) {
            }
            OSDisableInterrupts();
        } while (RunQueueBits == 0);
        OSClearContext(&IdleContext);
    }

    RunQueueHint = FALSE;

    priority = __cntlzw(RunQueueBits);
    queue = &RunQueue[priority];
    RemoveHead(queue, nextThread, link);
    if (queue->head == NULL) {
        RunQueueBits &= ~(1u << (OS_PRIORITY_MAX - priority));
    }
    nextThread->queue = NULL;
    nextThread->state = OS_THREAD_STATE_RUNNING;
    __OSSwitchThread(nextThread);
    return nextThread;
} // 0x8021A790

/*
 * __OSReschedule
 *
 * Switches threads if a scheduling change is pending.
 */
void __OSReschedule(void) {
    if (RunQueueHint) {
        SelectThread(FALSE);
    }
} // 0x8021A9C0

/*
 * OSYieldThread
 *
 * Gives the processor to the next ready thread of the same or higher priority.
 */
void OSYieldThread(void) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    SelectThread(TRUE);
    OSRestoreInterrupts(enabled);
} // 0x8021A9E0

/*
 * OSCreateThread
 *
 * Sets thread up to run func(param) on the given stack, suspended. stack is
 * the top of the stack, stackSize its size in bytes.
 */
BOOL OSCreateThread(OSThread* thread, OSThreadStartFunction func, void* param, void* stack, u32 stackSize,
                    OSPriority priority, u16 attr) {
    BOOL enabled;
    u32 sp;
    int i;

    if (priority < OS_PRIORITY_MIN || OS_PRIORITY_MAX < priority) {
        return FALSE;
    }

    thread->state = OS_THREAD_STATE_READY;
    thread->attr = attr & OS_THREAD_ATTR_DETACH;
    thread->base = priority;
    thread->priority = priority;
    thread->suspend = 1;
    thread->val = (void*)-1;
    thread->mutex = NULL;
    OSInitThreadQueue(&thread->queueJoin);
    thread->queueMutex.head = thread->queueMutex.tail = NULL;

    /* An 8-byte aligned initial frame with a null back chain and LR save word. */
    sp = (u32)stack;
    sp &= ~7;
    sp -= 8;
    ((u32*)sp)[0] = 0;
    ((u32*)sp)[1] = 0;
    OSInitContext(&thread->context, (u32)func, sp);
    thread->context.lr = (u32)OSExitThread;
    thread->context.gpr[3] = (u32)param;
    thread->stackBase = stack;
    thread->stackEnd = (u32*)((u32)stack - stackSize);
    *thread->stackEnd = OS_THREAD_STACK_MAGIC;
    thread->error = 0;
    for (i = 0; i < OS_THREAD_SPECIFIC_MAX; ++i) {
        thread->specific[i] = NULL;
    }

    enabled = OSDisableInterrupts();

    if (__OSErrorTable[OS_ERROR_FPE]) {
        /* Start with a defined FP state that traps, like OSSetErrorHandler gives running threads. */
        thread->context.srr1 |= MSR_FE0 | MSR_FE1;
        thread->context.state |= OS_CONTEXT_STATE_FPSAVED;
        thread->context.fpscr = (__OSFpscrEnableBits & FPSCR_ENABLE) | FPSCR_NI;
        for (i = 0; i < 32; ++i) {
            *(u64*)&thread->context.fpr[i] = 0xFFFFFFFFFFFFFFFFULL;
            *(u64*)&thread->context.psf[i] = 0xFFFFFFFFFFFFFFFFULL;
        }
    }

    AddTail(&__OSActiveThreadQueue, thread, linkActive);

    OSRestoreInterrupts(enabled);
    return TRUE;
} // 0x8021AA20

/*
 * OSExitThread
 *
 * Ends the running thread with exit value val. A detached thread is released
 * at once; any other waits, moribund, for OSJoinThread.
 */
void OSExitThread(void* val) {
    BOOL enabled;
    OSThread* currentThread;

    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();

    OSClearContext(&currentThread->context);
    if (currentThread->attr & OS_THREAD_ATTR_DETACH) {
        RemoveItem(&__OSActiveThreadQueue, currentThread, linkActive);
        currentThread->state = 0;
    } else {
        currentThread->state = OS_THREAD_STATE_MORIBUND;
        currentThread->val = val;
    }
    __OSUnlockAllMutex(currentThread);
    OSWakeupThread(&currentThread->queueJoin);
    RunQueueHint = TRUE;
    __OSReschedule();
    OSRestoreInterrupts(enabled);
} // 0x8021AC90

/*
 * OSCancelThread
 *
 * Ends thread from outside, wherever it is queued.
 */
void OSCancelThread(OSThread* thread) {
    BOOL enabled;

    enabled = OSDisableInterrupts();

    __OSCancelInternalAlarms(thread);

    switch (thread->state) {
    case OS_THREAD_STATE_READY:
        if (thread->suspend <= 0) {
            UnsetRun(thread);
        }
        break;
    case OS_THREAD_STATE_RUNNING:
        RunQueueHint = TRUE;
        break;
    case OS_THREAD_STATE_WAITING:
        RemoveItem(thread->queue, thread, link);
        thread->queue = NULL;
        if (thread->suspend <= 0 && thread->mutex) {
            UpdatePriority(thread->mutex->thread);
        }
        break;
    default:
        OSRestoreInterrupts(enabled);
        return;
    }

    OSClearContext(&thread->context);
    if (thread->attr & OS_THREAD_ATTR_DETACH) {
        RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
        thread->state = 0;
    } else {
        thread->state = OS_THREAD_STATE_MORIBUND;
    }
    __OSUnlockAllMutex(thread);
    OSWakeupThread(&thread->queueJoin);
    __OSReschedule();
    OSRestoreInterrupts(enabled);
} // 0x8021AD80

/*
 * OSJoinThread
 *
 * Waits for thread to exit, stores its exit value in *val and releases it.
 * Returns FALSE if thread is detached or another thread already joins it.
 */
BOOL OSJoinThread(OSThread* thread, void** val) {
    BOOL enabled;

    enabled = OSDisableInterrupts();

    if (!(thread->attr & OS_THREAD_ATTR_DETACH) && thread->state != OS_THREAD_STATE_MORIBUND &&
        thread->queueJoin.head == NULL) {
        OSSleepThread(&thread->queueJoin);
        if (!__OSIsThreadActive(thread)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
    }

    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        if (val) {
            *val = thread->val;
        }
        RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
        thread->state = 0;
        OSRestoreInterrupts(enabled);
        return TRUE;
    }

    OSRestoreInterrupts(enabled);
    return FALSE;
} // 0x8021AF60

/*
 * OSDetachThread
 *
 * Marks thread detached, releasing it now if it has already exited.
 */
void OSDetachThread(OSThread* thread) {
    BOOL enabled;

    enabled = OSDisableInterrupts();

    ASSERTMSG1(__OSIsThreadActive(thread), "OSDetachThread(): thread %p is not active.", thread);

    thread->attr |= OS_THREAD_ATTR_DETACH;
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        RemoveItem(&__OSActiveThreadQueue, thread, linkActive);
        thread->state = 0;
    }
    OSWakeupThread(&thread->queueJoin);
    OSRestoreInterrupts(enabled);
}

/*
 * OSResumeThread
 *
 * Drops the suspend count of thread and lets it run when it reaches zero.
 * Returns the previous suspend count.
 */
s32 OSResumeThread(OSThread* thread) {
    BOOL enabled;
    s32 suspendCount;

    enabled = OSDisableInterrupts();

    suspendCount = thread->suspend--;
    if (thread->suspend < 0) {
        thread->suspend = 0;
    } else if (thread->suspend == 0) {
        switch (thread->state) {
        case OS_THREAD_STATE_READY:
            thread->priority = __OSGetEffectivePriority(thread);
            SetRun(thread);
            break;
        case OS_THREAD_STATE_WAITING:
            RemoveItem(thread->queue, thread, link);
            thread->priority = __OSGetEffectivePriority(thread);
            AddPrio(thread->queue, thread, link);
            if (thread->mutex) {
                UpdatePriority(thread->mutex->thread);
            }
            break;
        }
        __OSReschedule();
    }

    OSRestoreInterrupts(enabled);
    return suspendCount;
} // 0x8021B0B0

/*
 * OSSuspendThread
 *
 * Raises the suspend count of thread, taking it off the run queues on the
 * first suspend. Returns the previous suspend count.
 */
s32 OSSuspendThread(OSThread* thread) {
    BOOL enabled;
    s32 suspendCount;

    enabled = OSDisableInterrupts();

    suspendCount = thread->suspend++;
    if (suspendCount == 0) {
        switch (thread->state) {
        case OS_THREAD_STATE_RUNNING:
            RunQueueHint = TRUE;
            thread->state = OS_THREAD_STATE_READY;
            break;
        case OS_THREAD_STATE_READY:
            UnsetRun(thread);
            break;
        case OS_THREAD_STATE_WAITING:
            /* A suspended waiter goes to the back, below every real priority. */
            RemoveItem(thread->queue, thread, link);
            thread->priority = OS_PRIORITY_MAX + 1;
            AddTail(thread->queue, thread, link);
            if (thread->mutex) {
                UpdatePriority(thread->mutex->thread);
            }
            break;
        }
        __OSReschedule();
    }

    OSRestoreInterrupts(enabled);
    return suspendCount;
} // 0x8021B350

/*
 * OSSleepThread
 *
 * Puts the running thread to sleep on queue, in priority order.
 */
void OSSleepThread(OSThreadQueue* queue) {
    BOOL enabled;
    OSThread* currentThread;

    enabled = OSDisableInterrupts();
    currentThread = OSGetCurrentThread();

    currentThread->state = OS_THREAD_STATE_WAITING;
    currentThread->queue = queue;
    AddPrio(queue, currentThread, link);
    RunQueueHint = TRUE;
    __OSReschedule();
    OSRestoreInterrupts(enabled);
} // 0x8021B4E0

/*
 * OSWakeupThread
 *
 * Readies every thread sleeping on queue.
 */
void OSWakeupThread(OSThreadQueue* queue) {
    BOOL enabled;
    OSThread* thread;

    enabled = OSDisableInterrupts();

    while (queue->head) {
        RemoveHead(queue, thread, link);
        thread->state = OS_THREAD_STATE_READY;
        if (thread->suspend <= 0) {
            SetRun(thread);
        }
    }
    __OSReschedule();
    OSRestoreInterrupts(enabled);
} // 0x8021B5D0

/*
 * OSSetThreadPriority
 *
 * Sets the base priority of thread. Returns FALSE if priority is out of range.
 */
BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    BOOL enabled;

    if (priority < OS_PRIORITY_MIN || OS_PRIORITY_MAX < priority) {
        return FALSE;
    }

    enabled = OSDisableInterrupts();

    if (thread->base != priority) {
        thread->base = priority;
        UpdatePriority(thread);
        __OSReschedule();
    }

    OSRestoreInterrupts(enabled);
    return TRUE;
}

/*
 * OSGetThreadPriority
 *
 * Returns the base priority of thread.
 */
OSPriority OSGetThreadPriority(OSThread* thread) {
    return thread->base;
}

/*
 * OSSetIdleFunction
 *
 * Runs idleFunction(param) in a thread of the lowest priority, or cancels the
 * idle thread when idleFunction is NULL. Returns the idle thread it started.
 */
OSThread* OSSetIdleFunction(OSIdleFunction idleFunction, void* param, void* stack, u32 stackSize) {
    if (idleFunction) {
        if (IdleThread.state == 0) {
            OSCreateThread(&IdleThread, (OSThreadStartFunction)idleFunction, param, stack, stackSize,
                           OS_PRIORITY_IDLE, OS_THREAD_ATTR_DETACH);
            OSResumeThread(&IdleThread);
            return &IdleThread;
        }
    } else if (IdleThread.state != 0) {
        OSCancelThread(&IdleThread);
    }
    return NULL;
}

/*
 * OSGetIdleFunction
 *
 * Returns the idle thread, or NULL if none runs.
 */
OSThread* OSGetIdleFunction(void) {
    if (IdleThread.state != 0) {
        return &IdleThread;
    }
    return NULL;
}

/*
 * CheckThreadQueue
 *
 * Returns TRUE if the links of queue are consistent.
 */
static BOOL CheckThreadQueue(OSThreadQueue* queue) {
    OSThread* thread;

    if (queue->head && queue->head->link.prev) {
        return FALSE;
    }
    if (queue->tail && queue->tail->link.next) {
        return FALSE;
    }
    for (thread = queue->head; thread; thread = thread->link.next) {
        if (thread->link.next && thread != thread->link.next->link.prev) {
            return FALSE;
        }
        if (thread->link.prev && thread != thread->link.prev->link.next) {
            return FALSE;
        }
    }
    return TRUE;
} // 0x8021B6D0

/*
 * IsMember
 *
 * Returns TRUE if thread is on queue.
 */
static BOOL IsMember(OSThreadQueue* queue, OSThread* thread) {
    OSThread* member;

    for (member = queue->head; member; member = member->link.next) {
        if (thread == member) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * OSCheckActiveThreads
 *
 * Checks the run queues and every active thread for consistency and halts on
 * the first failure. Returns the number of active threads.
 */
s32 OSCheckActiveThreads(void) {
    OSThread* thread;
    OSPriority prio;
    s32 cThread;
    BOOL enabled;

    cThread = 0;
    enabled = OSDisableInterrupts();

    for (prio = OS_PRIORITY_MIN; prio <= OS_PRIORITY_MAX; ++prio) {
        if (RunQueueBits & (1u << (OS_PRIORITY_MAX - prio))) {
            ASSERTREPORT(1522, RunQueue[prio].head != NULL && RunQueue[prio].tail != NULL);
        } else {
            ASSERTREPORT(1527, RunQueue[prio].head == NULL && RunQueue[prio].tail == NULL);
        }
        ASSERTREPORT(1529, CheckThreadQueue(&RunQueue[prio]));
    }

    ASSERTREPORT(1534, __OSActiveThreadQueue.head == NULL || __OSActiveThreadQueue.head->linkActive.prev == NULL);
    ASSERTREPORT(1536, __OSActiveThreadQueue.tail == NULL || __OSActiveThreadQueue.tail->linkActive.next == NULL);

    for (thread = __OSActiveThreadQueue.head; thread; thread = thread->linkActive.next) {
        ++cThread;
        ASSERTREPORT(1544, thread->linkActive.next == NULL || thread == thread->linkActive.next->linkActive.prev);
        ASSERTREPORT(1546, thread->linkActive.prev == NULL || thread == thread->linkActive.prev->linkActive.next);
        ASSERTREPORT(1549, *(thread->stackEnd) == OS_THREAD_STACK_MAGIC);
        ASSERTREPORT(1552, OS_PRIORITY_MIN <= thread->priority && thread->priority <= OS_PRIORITY_MAX+1);
        ASSERTREPORT(1553, 0 <= thread->suspend);
        ASSERTREPORT(1554, CheckThreadQueue(&thread->queueJoin));

        switch (thread->state) {
        case OS_THREAD_STATE_READY:
            if (thread->suspend <= 0) {
                ASSERTREPORT(1560, thread->queue == &RunQueue[thread->priority]);
                ASSERTREPORT(1561, IsMember(&RunQueue[thread->priority], thread));
                ASSERTREPORT(1562, thread->priority == __OSGetEffectivePriority(thread));
            }
            break;
        case OS_THREAD_STATE_RUNNING:
            ASSERTREPORT(1566, !IsSuspended(thread->suspend));
            ASSERTREPORT(1567, thread->queue == NULL);
            ASSERTREPORT(1568, thread->priority == __OSGetEffectivePriority(thread));
            break;
        case OS_THREAD_STATE_WAITING:
            ASSERTREPORT(1571, thread->queue != NULL);
            ASSERTREPORT(1572, CheckThreadQueue(thread->queue));
            ASSERTREPORT(1573, IsMember(thread->queue, thread));
            if (thread->suspend <= 0) {
                ASSERTREPORT(1576, thread->priority == __OSGetEffectivePriority(thread));
            } else {
                ASSERTREPORT(1580, thread->priority == 32);
            }
            ASSERTREPORT(1582, !__OSCheckDeadLock(thread));
            break;
        case OS_THREAD_STATE_MORIBUND:
            ASSERTREPORT(1586, thread->queueMutex.head == NULL && thread->queueMutex.tail == NULL);
            break;
        default:
            OSReport("OSCheckActiveThreads: Failed. unkown thread state (%d) of thread %p\n", thread->state, thread);
            OSPanic(__FILE__, 1592, "");
            break;
        }

        ASSERTREPORT(1597, __OSCheckMutexes(thread));
    }

    OSRestoreInterrupts(enabled);
    return cThread;
} // 0x8021B770

/*
 * OSClearStack
 *
 * Fills the unused part of the current thread's stack with val.
 */
void OSClearStack(u8 val) {
    u32 sp;
    u32* p;
    u32 pattern;

    pattern = ((u32)val << 24) | ((u32)val << 16) | ((u32)val << 8) | (u32)val;
    sp = OSGetStackPointer();
    for (p = __OSCurrentThread->stackEnd + 1; p < (u32*)sp; ++p) {
        *p = pattern;
    }
}

/*
 * OSSetThreadSpecific
 *
 * Stores ptr in slot index of the running thread.
 */
void OSSetThreadSpecific(s32 index, void* ptr) {
    OSThread* thread;

    thread = __OSCurrentThread;
    if (thread && 0 <= index && index < OS_THREAD_SPECIFIC_MAX) {
        thread->specific[index] = ptr;
    }
}

/*
 * OSGetThreadSpecific
 *
 * Returns slot index of the running thread.
 */
void* OSGetThreadSpecific(s32 index) {
    OSThread* thread;

    thread = __OSCurrentThread;
    if (thread && 0 <= index && index < OS_THREAD_SPECIFIC_MAX) {
        return thread->specific[index];
    }
    return NULL;
}

/*
 * SleepAlarmHandler
 *
 * Wakes the thread OSSleepTicks suspended, if it still exists.
 */
static void SleepAlarmHandler(OSAlarm* alarm, OSContext* context) {
    if (__OSIsThreadActive((OSThread*)OSGetAlarmUserData(alarm))) {
        OSResumeThread((OSThread*)OSGetAlarmUserData(alarm));
    }
} // 0x8021BED0

/*
 * OSSleepTicks
 *
 * Suspends the running thread for ticks timebase ticks.
 */
void OSSleepTicks(OSTime ticks) {
    BOOL enabled;
    OSThread* currentThread;
    OSAlarm alarm;

    enabled = OSDisableInterrupts();

    currentThread = __OSCurrentThread;
    if (currentThread == NULL) {
        OSRestoreInterrupts(enabled);
        return;
    }

    OSCreateAlarm(&alarm);
    __OSSetInternalAlarmUserData(&alarm, currentThread);
    OSSetAlarm(&alarm, ticks, SleepAlarmHandler);
    OSSuspendThread(currentThread);
    OSCancelAlarm(&alarm);
    OSRestoreInterrupts(enabled);
} // 0x8021BF50
