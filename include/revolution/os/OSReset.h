#ifndef REVOLUTION_OS_OSRESET_H
#define REVOLUTION_OS_OSRESET_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef BOOL (*OSShutdownFunction)(BOOL final, u32 event);

typedef struct OSShutdownFunctionInfo {
    OSShutdownFunction func;
    u32 priority;               /* lower runs first */
    struct OSShutdownFunctionInfo* next;
    struct OSShutdownFunctionInfo* prev;
} OSShutdownFunctionInfo;

typedef struct OSShutdownFunctionQueue {
    OSShutdownFunctionInfo* head;
    OSShutdownFunctionInfo* tail;
} OSShutdownFunctionQueue;

/* TRUE once the idle-mode shutdown has chosen to return to the system menu instead of standby. */
extern BOOL __OSIsReturnToIdle;

void OSRegisterShutdownFunction(OSShutdownFunctionInfo* info);
BOOL __OSCallShutdownFunctions(BOOL final, u32 event);
void __OSShutdownDevices(u32 event);
void OSShutdownSystem(void);
void OSRestart(u32 resetCode);
void OSReturnToMenu(void);
void OSReturnToDataManager(void);
void __OSReturnToMenuForError(void);
void __OSHotResetForError(void);
u32 OSGetResetCode(void);
void OSResetSystem(int reset, u32 resetCode, int forceMenu);

/* Flag in the reset code that marks a restart (as opposed to a cold boot). */
#define OS_RESET_RESTART 0x80000000

/* TRUE when the program was started by a restart (reset code bit 31) rather than a cold boot. */
#define OSIsRestart() ((OSGetResetCode() & OS_RESET_RESTART) ? TRUE : FALSE)

#ifdef __cplusplus
}
#endif

#endif
