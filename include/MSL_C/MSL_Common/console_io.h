#ifndef _MSL_CONSOLE_IO_H
#define _MSL_CONSOLE_IO_H

#include <MSL_C/MSL_Common/ansi_files.h>

#ifdef __cplusplus
extern "C" {
#endif

int __read_console(__file_handle handle, unsigned char* buffer, size_t* count, void* idle_proc);
int __write_console(__file_handle handle, unsigned char* buffer, size_t* count, void* idle_proc);
int __close_console(__file_handle handle);

/* The debugger's console output, in MetroTRK (mslsupp.c). */
int __TRK_write_console(__file_handle handle, unsigned char* buffer, size_t* count, void* idle_proc);

#ifdef __cplusplus
}
#endif

#endif
