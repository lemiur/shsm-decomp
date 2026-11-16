#ifndef REVOLUTION_OS_OSPLAYRECORD_H
#define REVOLUTION_OS_OSPLAYRECORD_H

#include <revolution/types.h>
#include <revolution/os/OSAlarm.h>
#include <revolution/os/OSTime.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Contents of /title/00000001/00000002/data/play_rec.dat. */
typedef struct OSPlayRecord {
    u32 checkSum;
    u16 titleName[2][21];
    OSTime playStartTime;
    OSTime playTime;
    char gameCode[4];
    char companyCode[2];
    char res[18];
} OSPlayRecord;

u32 RecordCheckSum(void);
void PlayRecordAlarmCallback(OSAlarm* alarm, OSContext* context);
void __OSStartPlayRecord(void);
void __OSStopPlayRecord(void);

#ifdef __cplusplus
}
#endif

#endif
