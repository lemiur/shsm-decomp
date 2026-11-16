#ifndef REVOLUTION_OS_OSTIME_H
#define REVOLUTION_OS_OSTIME_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef s64 OSTime;
typedef u32 OSTick;

/* Bus clock in Hz, stored in low memory by the boot code. */
u32 __OSBusClock : 0x800000F8;

#define OS_BUS_CLOCK __OSBusClock
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)

#define OSSecondsToTicks(sec) ((sec) * (OS_TIMER_CLOCK))
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))
#define OSMicrosecondsToTicks(usec) (((usec) * (OS_TIMER_CLOCK / 125000)) / 8)
#define OSNanosecondsToTicks(nsec) (((nsec) * (OS_TIMER_CLOCK / 125000)) / 8000)
#define OSTicksToSeconds(ticks) ((ticks) / OS_TIMER_CLOCK)
#define OSTicksToMilliseconds(ticks) ((ticks) / (OS_TIMER_CLOCK / 1000))
#define OSTicksToMicroseconds(ticks) (((ticks) * 8) / (OS_TIMER_CLOCK / 125000))
/* Difference of two times; the SDK truncates to 32 bits, as the menu's timeout tests compile. */
#define OSDiffTick(tick1, tick0) ((OSTick)(tick1) - (OSTick)(tick0))

typedef struct OSCalendarTime {
    int sec;    /* 0-61 */
    int min;    /* 0-59 */
    int hour;   /* 0-23 */
    int mday;   /* 1-31 */
    int mon;    /* 0-11 */
    int year;   /* 0- */
    int wday;   /* 0-6, Sunday first */
    int yday;   /* 0-365 */
    int msec;   /* 0-999 */
    int usec;   /* 0-999 */
} OSCalendarTime;

OSTime OSGetTime(void);
OSTick OSGetTick(void);
OSTime __OSGetSystemTime(void);
OSTime __OSTimeToSystemTime(OSTime time);
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td);

#ifdef __cplusplus
}
#endif

#endif
