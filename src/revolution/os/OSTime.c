#include <revolution/os.h>

#define USEC_MAX 1000
#define MSEC_MAX 1000
#define MONTH_MAX 12
#define WEEKDAY_MAX 7
#define YEAR_DAY_MAX 365
#define SECS_IN_MIN 60
#define SECS_IN_HOUR (SECS_IN_MIN * 60)
#define SECS_IN_DAY (SECS_IN_HOUR * 24)

/* Days from 1 Jan 0000 to 1 Jan 2000, the epoch of the time base. */
#define BIAS (2000 * 365 + (2000 + 3) / 4 - (2000 - 1) / 100 + (2000 - 1) / 400)

/* Offset added to the time base to give system time (low memory). */
#define OS_SYSTEM_TIME_ADJUST 0x30D8

/* Days before the first of each month. */
static int YearDays[MONTH_MAX] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static int LeapYearDays[MONTH_MAX] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

/*
 * OSGetTime
 *
 * Reads the 64-bit time base, retrying if the upper half ticked over
 * between the two reads.
 */
asm OSTime OSGetTime(void) {
    nofralloc
@retry:
    mftbu r3
    mftb r4
    mftbu r5
    cmpw r3, r5
    bne @retry
    blr
} // 0x8021C000

asm OSTick OSGetTick(void) {
    nofralloc
    mftb r3
    blr
} // 0x8021C020

/*
 * __OSGetSystemTime
 */
OSTime __OSGetSystemTime(void) {
    BOOL enabled;
    OSTime* timeAdjustAddr = (OSTime*)OSPhysicalToCached(OS_SYSTEM_TIME_ADJUST);
    OSTime result;

    enabled = OSDisableInterrupts();
    result = *timeAdjustAddr + OSGetTime();
    OSRestoreInterrupts(enabled);

    return result;
} // 0x8021C030

/*
 * __OSTimeToSystemTime
 */
OSTime __OSTimeToSystemTime(OSTime time) {
    BOOL enabled;
    OSTime* timeAdjustAddr = (OSTime*)OSPhysicalToCached(OS_SYSTEM_TIME_ADJUST);
    OSTime result;

    enabled = OSDisableInterrupts();
    result = *timeAdjustAddr + time;
    OSRestoreInterrupts(enabled);

    return result;
} // 0x8021C0A0

static BOOL IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

/* Leap days in the years before year. */
static int GetLeapDays(int year) {
    if (year < 1) {
        return 0;
    }
    return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

/* Fills in the date fields of cal from a day count since 1 Jan 0000. */
static void GetDates(int days, OSCalendarTime* cal) {
    int year;
    int n;
    int month;
    int* p_days;

    cal->wday = (days + 6) % WEEKDAY_MAX;

    for (year = days / YEAR_DAY_MAX; days < (n = year * YEAR_DAY_MAX + GetLeapDays(year)); year--) {
        ;
    }
    days -= n;
    cal->year = year;
    cal->yday = days;

    p_days = IsLeapYear(year) ? LeapYearDays : YearDays;
    for (month = MONTH_MAX; days < p_days[--month];) {
        ;
    }
    cal->mon = month;
    cal->mday = days - p_days[month] + 1;
}

/*
 * OSTicksToCalendarTime
 */
void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* td) {
    int days;
    int secs;
    OSTime d;

    d = ticks % OSSecondsToTicks(1);
    if (d < 0) {
        d += OSSecondsToTicks(1);
    }

    td->usec = (int)(OSTicksToMicroseconds(d) % USEC_MAX);
    td->msec = (int)(OSTicksToMilliseconds(d) % MSEC_MAX);

    ticks -= d;
    days = (int)(OSTicksToSeconds(ticks) / SECS_IN_DAY + BIAS);
    secs = (int)(OSTicksToSeconds(ticks) % SECS_IN_DAY);
    if (secs < 0) {
        days -= 1;
        secs += SECS_IN_DAY;
    }

    GetDates(days, td);
    td->hour = secs / 60 / 60;
    td->min = secs / 60 % 60;
    td->sec = secs % 60;
} // 0x8021C100
