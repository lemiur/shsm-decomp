#ifndef _MSL_WCHAR_H
#define _MSL_WCHAR_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef wchar_t wint_t;

size_t wcslen(const wchar_t* str);
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src);
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n);
wchar_t* wcsncat(wchar_t* dst, const wchar_t* src, size_t n);
int wcscmp(const wchar_t* str1, const wchar_t* str2);
wchar_t* wcschr(const wchar_t* str, wchar_t chr);
wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t n);
wchar_t* wmemchr(const wchar_t* s, wchar_t c, int n);
int swprintf(wchar_t* buffer, size_t n, const wchar_t* format, ...);
int vswprintf(wchar_t* buffer, size_t n, const wchar_t* format, va_list arg);

#ifdef __cplusplus
}
#endif

#endif
