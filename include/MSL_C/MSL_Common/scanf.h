#ifndef _MSL_SCANF_H
#define _MSL_SCANF_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Actions passed to a scanf read procedure. */
#define __GetAChar 0
#define __UngetAChar 1
#define __TestForError 2

typedef int (*__ReadProc)(void* read_arg, int ch, int action);

typedef struct {
    char* NextChar;
    int NullCharDetected;
} __InStrCtrl;

int __sformatter(__ReadProc read_proc, void* read_arg, const char* format, va_list arg, int is_secure);
int __StringRead(void* read_arg, int ch, int action);

#ifdef __cplusplus
}
#endif

#endif
