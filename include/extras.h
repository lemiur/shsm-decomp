#ifndef _MSL_EXTRAS_H
#define _MSL_EXTRAS_H

// Metrowerks non-ANSI string extensions.

#ifdef __cplusplus
extern "C" {
#endif

char* strlwr(char* str);
int stricmp(const char* str1, const char* str2);
char* strupr(char* str);

#ifdef __cplusplus
}
#endif

#endif
