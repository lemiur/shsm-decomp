#ifndef _MSL_STDIO_H
#define _MSL_STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <MSL_C/MSL_Common/ansi_files.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define stdin (&__files[0])
#define stdout (&__files[1])
#define stderr (&__files[2])

int fclose(FILE* file);
int fflush(FILE* file);
long ftell(FILE* file);
long _ftell(FILE* file);
int _fseek(FILE* file, fpos_t offset, int mode);
size_t __fwrite(const void* buffer, size_t size, size_t count, FILE* file);

int printf(const char* format, ...);
int vprintf(const char* format, va_list arg);
int vsprintf(char* buffer, const char* format, va_list arg);
int vsnprintf(char* buffer, size_t n, const char* format, va_list arg);
int sprintf(char* buffer, const char* format, ...);
int snprintf(char* buffer, size_t n, const char* format, ...);
int sscanf(const char* buffer, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
