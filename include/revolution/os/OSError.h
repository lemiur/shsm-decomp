#ifndef REVOLUTION_OS_OSERROR_H
#define REVOLUTION_OS_OSERROR_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef u16 OSError;
typedef void (*OSErrorHandler)(OSError error, OSContext* context, ...);

#define OS_ERROR_MACHINE_CHECK 1
#define OS_ERROR_DECREMENTER   8
#define OS_ERROR_PROTECTION    15
#define OS_ERROR_FPE           16
#define OS_ERROR_MAX           17

extern OSErrorHandler __OSErrorTable[OS_ERROR_MAX];
extern u32 __OSFpscrEnableBits;

void OSReport(const char* msg, ...);
void OSPanic(const char* file, int line, const char* msg, ...);

/* SDK assertions; they compile to nothing outside debug builds. */
#ifdef _DEBUG
#define ASSERTMSG(exp, msg) (void)((exp) || (OSPanic(__FILE__, __LINE__, (msg)), 0))
#define ASSERTMSG1(exp, msg, param1) (void)((exp) || (OSPanic(__FILE__, __LINE__, (msg), (param1)), 0))
#define ASSERT(exp) ASSERTMSG(exp, "Failed assertion " #exp)
#define ASSERTLINE(exp, line) (void)((exp) || (OSPanic(__FILE__, (line), "Failed assertion " #exp), 0))
#else
#define ASSERTMSG(exp, msg) ((void)0)
#define ASSERTMSG1(exp, msg, param1) ((void)0)
#define ASSERT(exp) ((void)0)
#define ASSERTLINE(exp, line) ((void)0)
#endif

OSErrorHandler OSSetErrorHandler(OSError error, OSErrorHandler handler);

#ifdef __cplusplus
}
#endif

#endif
