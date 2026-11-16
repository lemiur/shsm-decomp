#ifndef _MSL_WCHAR_IO_H
#define _MSL_WCHAR_IO_H

#include <MSL_C/MSL_Common/ansi_files.h>

#ifdef __cplusplus
extern "C" {
#endif

int fwide(FILE* file, int mode);

#ifdef __cplusplus
}
#endif

#endif
