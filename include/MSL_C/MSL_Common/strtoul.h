#ifndef _MSL_STRTOUL_H
#define _MSL_STRTOUL_H

#include <MSL_C/MSL_Common/scanf.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned long __strtoul(int base, int max_width, __ReadProc read_proc, void* read_arg, int* chars_scanned, int* negative,
                        int* overflow);
unsigned long long __strtoull(int base, int max_width, __ReadProc read_proc, void* read_arg, int* chars_scanned,
                              int* negative, int* overflow);

#ifdef __cplusplus
}
#endif

#endif
