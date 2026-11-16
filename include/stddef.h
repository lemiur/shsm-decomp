#ifndef _MSL_STDDEF_H
#define _MSL_STDDEF_H

typedef unsigned long size_t;
typedef long ptrdiff_t;

#ifndef __cplusplus
typedef unsigned short wchar_t;
#endif

#ifndef NULL
#define NULL 0L
#endif

#define offsetof(type, member) ((size_t) & (((type*)0)->member))

#endif
