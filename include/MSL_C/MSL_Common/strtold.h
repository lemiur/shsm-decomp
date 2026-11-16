#ifndef _MSL_STRTOLD_H
#define _MSL_STRTOLD_H

#include <MSL_C/MSL_Common/scanf.h>

#ifdef __cplusplus
extern "C" {
#endif

long double __strtold(int max_width, __ReadProc read_proc, void* read_arg, int* chars_scanned, int* overflow);

#ifdef __cplusplus
}
#endif

#endif
