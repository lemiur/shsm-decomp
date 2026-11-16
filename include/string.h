#ifndef _MSL_STRING_H
#define _MSL_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int val, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memchr(const void* src, int val, size_t n);
void* __memrchr(const void* src, int val, size_t n);
int memcmp(const void* src1, const void* src2, size_t n);

size_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, size_t n);
char* strcat(char* dst, const char* src);
char* strncat(char* dst, const char* src, size_t n);
char* strchr(const char* str, int chr);
char* strstr(const char* str, const char* pat);
char* strtok(char* str, const char* set);

#ifdef __cplusplus
}
#endif

#endif
