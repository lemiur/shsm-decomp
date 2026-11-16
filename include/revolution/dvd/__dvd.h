// Reference: zeldaret/tp libs/revolution/src/dvd/__dvd.h
//
// DVD-internal declarations shared by the DVD units. Each function and global is declared once, here or in dvd.h.
#ifndef REVOLUTION_DVD_INTERNAL_H
#define REVOLUTION_DVD_INTERNAL_H

#include <revolution/types.h>
#include <revolution/os.h>
#include <revolution/os/OS.h>
#include <revolution/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif

// DVD
DVDCommandChecker __DVDSetOptionalCommandChecker(DVDCommandChecker func);
void __DVDSetImmCommand(u32 command);
void __DVDAudioBufferConfig(DVDCommandBlock* block, u32 enable, u32 size, DVDCBCallback callback);
BOOL __DVDLowBreak(void);
BOOL __DVDStopMotorAsync(DVDCommandBlock* block, DVDCBCallback callback);
void __DVDRestartMotor(void);
extern DVDCommandBlock __DVDStopMotorCommandBlock; // defined in dvd.c
u32 __DVDGetCoverStatus(void);
void __DVDPrepareReset(void);
void __DVDPrepareResetAsync(DVDCBCallback callback);

extern vu32 __DVDLayoutFormat; // volatile: stateReadingFST (dvd.c) reloads the format at each of its four uses; a plain u32 merges the loads

// DVD ERROR
void __DVDStoreErrorCode(u32 error, DVDCBCallback callback);
extern DVDErrorInfo __ErrorInfo; // defined in dvderror.c

// DVD FATAL
void __DVDPrintFatalMessage(void);
void __DVDShowFatalMessage(void);
extern const char* const __DVDErrorMessageDefault[];   // defined in dvdFatal.c
extern const char* const __DVDErrorMessageEurope[];    // defined in dvdFatal.c
extern const char* __DVDErrorMessageChinaKorea[];      // defined in dvdFatal.c

// DVD DEVICE ERROR
extern const char* const __DVDDeviceErrorMessage[]; // defined in dvdDeviceError.c

// DVD FS
extern OSThreadQueue __DVDThreadQueue;

void __DVDFSInit(void);

// DVD BROADWAY
int __DVDLowTestAlarm(const OSAlarm* alarm);

// DVD QUEUE
void __DVDClearWaitingQueue(void);
int __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block);
DVDCommandBlock* __DVDPopWaitingQueue(void);
int __DVDCheckWaitingQueue(void);
int __DVDDequeueWaitingQueue(DVDCommandBlock* block);
int __DVDIsBlockInWaitingQueue(DVDCommandBlock* block);
DVDCommandBlock* __DVDGetNextWaitingQueue(void);

#ifdef __cplusplus
}
#endif

#endif
