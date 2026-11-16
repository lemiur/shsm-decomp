#ifndef _MSL_STDLIB_H
#define _MSL_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

void exit(int status);
char* getenv(const char* name);
double strtod(const char* str, char** end);
long strtol(const char* str, char** end, int base);
int atoi(const char* str);
long atol(const char* str);

typedef int (*_compare_function)(const void*, const void*);

void qsort(void* table_base, size_t num_members, size_t member_size, _compare_function compare_members);

int rand(void);
void srand(unsigned int seed);

void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

int mbtowc(wchar_t* pwc, const char* s, size_t n);
size_t mbstowcs(wchar_t* pwcs, const char* s, size_t n);
size_t wcstombs(char* s, const wchar_t* pwcs, size_t n);

#ifdef __cplusplus
}
#endif

#endif
