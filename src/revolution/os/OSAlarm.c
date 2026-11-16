#include <revolution/os.h>
#include <revolution/base.h>
#include <revolution/dvd.h>

/* Tag of alarms set through __OSSetInternalAlarmUserData. */
#define ALARM_TAG_INTERNAL 0xFFFFFFFF

/* Pending alarms, sorted by fire time. */
static struct OSAlarmQueue {
    OSAlarm* head;
    OSAlarm* tail;
} AlarmQueue;

static void DecrementerExceptionHandler(register __OSException exception, register OSContext* context);
static BOOL OnReset(BOOL final, u32 event);

static OSShutdownFunctionInfo ShutdownFunctionInfo = {OnReset, 0xFFFFFFFF};

/*
 * SetTimer
 *
 * Programs the decrementer to expire when alarm fires, clamped to the
 * positive range of the decrementer.
 */
static void SetTimer(OSAlarm* alarm) {
    OSTime delta;

    delta = alarm->fire - __OSGetSystemTime();
    if (delta < 0) {
        PPCMtdec(0);
    } else if (delta < 0x80000000) {
        PPCMtdec((u32)delta);
    } else {
        PPCMtdec(0x7FFFFFFF);
    }
}

/*
 * __OSInitAlarm
 */
void __OSInitAlarm(void) {
    if (__OSGetExceptionHandler(__OS_EXCEPTION_DECREMENTER) != DecrementerExceptionHandler) {
        AlarmQueue.head = AlarmQueue.tail = NULL;
        __OSSetExceptionHandler(__OS_EXCEPTION_DECREMENTER, DecrementerExceptionHandler);
        OSRegisterShutdownFunction(&ShutdownFunctionInfo);
    }
} // 0x80211DD0

void OSCreateAlarm(OSAlarm* alarm) {
    alarm->handler = NULL;
    alarm->tag = 0;
} // 0x80211E30

/*
 * InsertAlarm
 *
 * Queues alarm in fire-time order. A periodic alarm ignores fire and fires
 * at the first multiple of its period after start that is still ahead.
 */
static void InsertAlarm(OSAlarm* alarm, OSTime fire, OSAlarmHandler handler) {
    OSAlarm* next;
    OSAlarm* prev;

    if (0 < alarm->period) {
        OSTime time = __OSGetSystemTime();

        fire = alarm->start;
        if (alarm->start < time) {
            fire += alarm->period * ((time - alarm->start) / alarm->period + 1);
        }
    }

    alarm->handler = handler;
    alarm->fire = fire;

    for (next = AlarmQueue.head; next; next = next->next) {
        if (next->fire <= fire) {
            continue;
        }

        alarm->prev = next->prev;
        next->prev = alarm;
        alarm->next = next;
        prev = alarm->prev;
        if (prev) {
            prev->next = alarm;
        } else {
            AlarmQueue.head = alarm;
            SetTimer(alarm);
        }
        return;
    }

    alarm->next = NULL;
    prev = AlarmQueue.tail;
    AlarmQueue.tail = alarm;
    alarm->prev = prev;
    if (prev) {
        prev->next = alarm;
    } else {
        AlarmQueue.head = AlarmQueue.tail = alarm;
        SetTimer(alarm);
    }
} // 0x80211E40

/*
 * OSSetAlarm
 *
 * Fires handler once, tick time units from now.
 */
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    alarm->period = 0;
    InsertAlarm(alarm, __OSGetSystemTime() + tick, handler);
    OSRestoreInterrupts(enabled);
} // 0x80212090

/*
 * OSSetPeriodicAlarm
 *
 * Fires handler at start and every period after it.
 */
void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    BOOL enabled;

    enabled = OSDisableInterrupts();
    alarm->period = period;
    alarm->start = __OSTimeToSystemTime(start);
    InsertAlarm(alarm, 0, handler);
    OSRestoreInterrupts(enabled);
} // 0x80212100

/*
 * OSCancelAlarm
 */
void OSCancelAlarm(OSAlarm* alarm) {
    OSAlarm* next;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    if (alarm->handler == NULL) {
        OSRestoreInterrupts(enabled);
        return;
    }

    next = alarm->next;
    if (next == NULL) {
        AlarmQueue.tail = alarm->prev;
    } else {
        next->prev = alarm->prev;
    }
    if (alarm->prev) {
        alarm->prev->next = next;
    } else {
        AlarmQueue.head = next;
        if (next) {
            SetTimer(next);
        }
    }
    alarm->handler = NULL;

    OSRestoreInterrupts(enabled);
} // 0x80212190

/*
 * DecrementerExceptionCallback
 *
 * Pops the expired alarm, re-queues it if periodic, and runs its handler
 * on a fresh context with the scheduler held off. Does not return: it
 * resumes the interrupted context.
 */
static void DecrementerExceptionCallback(register __OSException exception, register OSContext* context) {
    OSAlarm* alarm;
    OSAlarm* next;
    OSAlarmHandler handler;
    OSTime time;
    OSContext exceptionContext;

    time = __OSGetSystemTime();
    alarm = AlarmQueue.head;
    if (alarm == NULL) {
        OSLoadContext(context);
    }

    if (time < alarm->fire) {
        SetTimer(alarm);
        OSLoadContext(context);
    }

    next = alarm->next;
    AlarmQueue.head = next;
    if (next == NULL) {
        AlarmQueue.tail = NULL;
    } else {
        next->prev = NULL;
    }

    handler = alarm->handler;
    alarm->handler = NULL;
    if (0 < alarm->period) {
        InsertAlarm(alarm, 0, handler);
    }

    if (AlarmQueue.head) {
        SetTimer(AlarmQueue.head);
    }

    OSDisableScheduler();
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    handler(alarm, context);
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
    OSEnableScheduler();
    __OSReschedule();
    OSLoadContext(context);
} // 0x802122B0

/*
 * DecrementerExceptionHandler
 *
 * Saves the registers the first-level handler left live into context,
 * then continues in C.
 */
static asm void DecrementerExceptionHandler(register __OSException exception, register OSContext* context) {
    nofralloc
    stw r0, OS_CONTEXT_R0(context)
    stw r1, OS_CONTEXT_R1(context)
    stw r2, OS_CONTEXT_R2(context)
    stmw r6, OS_CONTEXT_R6(context)
    mfspr r0, GQR1
    stw r0, OS_CONTEXT_GQR1(context)
    mfspr r0, GQR2
    stw r0, OS_CONTEXT_GQR2(context)
    mfspr r0, GQR3
    stw r0, OS_CONTEXT_GQR3(context)
    mfspr r0, GQR4
    stw r0, OS_CONTEXT_GQR4(context)
    mfspr r0, GQR5
    stw r0, OS_CONTEXT_GQR5(context)
    mfspr r0, GQR6
    stw r0, OS_CONTEXT_GQR6(context)
    mfspr r0, GQR7
    stw r0, OS_CONTEXT_GQR7(context)
    stwu r1, -8(r1)
    b DecrementerExceptionCallback
} // 0x802124E0

/*
 * OnReset
 *
 * Shutdown hook: on the final pass, cancels every alarm except the ones
 * DVD still needs.
 */
static BOOL OnReset(BOOL final, u32 event) {
    OSAlarm* alarm;
    OSAlarm* next;

    if (final != FALSE) {
        alarm = AlarmQueue.head;
        next = (alarm) ? alarm->next : NULL;
        while (alarm != NULL) {
            if (__DVDTestAlarm(alarm) == FALSE) {
                OSCancelAlarm(alarm);
            }
            alarm = next;
            next = (alarm) ? alarm->next : NULL;
        }
    }

    return TRUE;
} // 0x80212530

void OSSetAlarmUserData(OSAlarm* alarm, void* data) {
    alarm->userData = data;
} // 0x802125C0

void* OSGetAlarmUserData(const OSAlarm* alarm) {
    return alarm->userData;
} // 0x802125D0

/*
 * __OSSetInternalAlarmUserData
 *
 * Sets the user data and marks the alarm internal, so that __OSCancelInternalAlarms
 * can find it.
 *
 * Name: Gormiti link map (os.a OSAlarm.o).
 */
void __OSSetInternalAlarmUserData(OSAlarm* alarm, void* data) {
    alarm->userData = data;
    alarm->tag = ALARM_TAG_INTERNAL;
} // 0x802125E0

/*
 * __OSCancelInternalAlarms
 *
 * Cancels every internal alarm whose user data is data.
 */
void __OSCancelInternalAlarms(void* data) {
    BOOL enabled;
    OSAlarm* alarm;
    OSAlarm* next;

    enabled = OSDisableInterrupts();

    alarm = AlarmQueue.head;
    next = (alarm) ? alarm->next : NULL;
    while (alarm != NULL) {
        if (alarm->tag == ALARM_TAG_INTERNAL && alarm->userData == data) {
            OSCancelAlarm(alarm);
        }
        alarm = next;
        next = (alarm) ? alarm->next : NULL;
    }

    OSRestoreInterrupts(enabled);
} // 0x802125F0
