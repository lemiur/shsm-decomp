#ifndef REVOLUTION_OS_OSEXCEPTION_H
#define REVOLUTION_OS_OSEXCEPTION_H

#include <revolution/types.h>
#include <revolution/os/OSContext.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef u8 __OSException;
typedef void (*__OSExceptionHandler)(__OSException exception, OSContext* context);

#define __OS_EXCEPTION_DSI            2
#define __OS_EXCEPTION_ISI            3
#define __OS_EXCEPTION_EXTERNAL_INTERRUPT 4
#define __OS_EXCEPTION_ALIGNMENT      5
#define __OS_EXCEPTION_PROGRAM        6
#define __OS_EXCEPTION_FLOATING_POINT 7
#define __OS_EXCEPTION_DECREMENTER    8

/* Number of first-level exception vectors the OS installs. */
#define __OS_EXCEPTION_MAX            15

__OSExceptionHandler __OSSetExceptionHandler(__OSException exception, __OSExceptionHandler handler);
__OSExceptionHandler __OSGetExceptionHandler(__OSException exception);
void __OSUnhandledException(__OSException exception, OSContext* context, u32 dsisr, u32 dar);

#ifdef __cplusplus
}
#endif

#endif
