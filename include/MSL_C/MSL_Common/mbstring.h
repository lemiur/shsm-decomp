#ifndef _MSL_MBSTRING_H
#define _MSL_MBSTRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int __mbtowc_noconv(wchar_t* pwc, const char* s, size_t n);
int __wctomb_noconv(char* s, wchar_t wchar);

#ifdef __cplusplus
}
#endif

#endif
