#ifndef _MSL_BUFFER_IO_H
#define _MSL_BUFFER_IO_H

#include <MSL_C/MSL_Common/ansi_files.h>

#ifdef __cplusplus
extern "C" {
#endif

void __prep_buffer(FILE* file);
int __flush_buffer(FILE* file, size_t* bytes_flushed);

#ifdef __cplusplus
}
#endif

#endif
