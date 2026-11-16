#ifndef REVOLUTION_OS_OSALARM_H
#define REVOLUTION_OS_OSALARM_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSTime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSAlarm OSAlarm;
typedef void (*OSAlarmHandler)(OSAlarm* alarm, OSContext* context);

struct OSAlarm {
    OSAlarmHandler handler;     /* NULL while the alarm is not queued */
    u32 tag;
    OSTime fire;                /* system time of the next expiry */
    OSAlarm* prev;
    OSAlarm* next;
    OSTime period;              /* 0 for a one-shot alarm */
    OSTime start;               /* system time of the first expiry of a periodic alarm */
    void* userData;
};

void __OSInitAlarm(void);
void OSCreateAlarm(OSAlarm* alarm);
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler);
void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm* alarm);
void OSSetAlarmUserData(OSAlarm* alarm, void* data);
void* OSGetAlarmUserData(const OSAlarm* alarm);

/* The user data with its type spelled at the access, as the SDK's own callers write it. */
#define OSGetAlarmUserDataAny(type_, alarm_) ((type_)(OSGetAlarmUserData(alarm_)))
#define OSSetAlarmUserDataAny(alarm_, data_) OSSetAlarmUserData(alarm_, (void*)(data_))
void __OSSetInternalAlarmUserData(OSAlarm* alarm, void* data);
void __OSCancelInternalAlarms(void* data);

#ifdef __cplusplus
}
#endif

#endif
