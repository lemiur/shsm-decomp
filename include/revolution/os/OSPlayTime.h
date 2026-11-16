#ifndef REVOLUTION_OS_OSPLAYTIME_H
#define REVOLUTION_OS_OSPLAYTIME_H

#include <revolution/types.h>
#include <revolution/esp.h>
#include <revolution/os/OSAlarm.h>
#include <revolution/os/OSTime.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What kind of limit a ticket puts on the play time (the last two names are invented). */
typedef enum __OSPlayTimeType {
    OSPLAYTIME_PERMANENT = 0,
    OSPLAYTIME_LIMITED = 1,
    OSPLAYTIME_LAUNCHES = 4,
    OSPLAYTIME_OTHER = 9
} __OSPlayTimeType;

typedef void (*OSPlayTimeCallbackFunc)(void);

/* The alarm that fires when the play time runs out, and the time it fires at (zero when unlimited). */
extern OSAlarm __OSExpireAlarm;
extern OSTime __OSExpireTime;
extern OSPlayTimeCallbackFunc __OSExpireCallback;
extern BOOL __OSExpireSetExpiredFlag;

BOOL OSPlayTimeIsLimited(void);
void __OSPlayTimeFadeLastAIDCallback(void);
BOOL __OSWriteExpiredFlag(void);
BOOL __OSWriteExpiredFlagIfSet(void);
s32 __OSGetPlayTime(ESTicketView* ticket, __OSPlayTimeType* type, u32* playTime);
s32 __OSGetPlayTimeCurrent(__OSPlayTimeType* type, u32* playTime);
void __OSInitPlayTime(void);

#ifdef __cplusplus
}
#endif

#endif
