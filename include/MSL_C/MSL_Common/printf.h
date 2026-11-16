#ifndef _MSL_PRINTF_H
#define _MSL_PRINTF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Conversion-specification options shared by printf.c and wprintf.c. */
enum justification_options {
    left_justification,
    right_justification,
    zero_fill
};

enum sign_options {
    only_minus,
    sign_always,
    space_holder
};

enum argument_options {
    normal_argument,
    char_argument,
    short_argument,
    long_argument,
    long_long_argument,
    wchar_argument,
    intmax_argument,
    size_t_argument,
    ptrdiff_argument,
    long_double_argument
};

/* Destination of the string printers (sprintf family): the buffer, its capacity and the count so far. */
typedef struct {
    char* CharStr;
    size_t MaxCharCount;
    size_t CharsWritten;
} __OutStrCtrl;

/* Destination of the wide string printers (swprintf family). */
typedef struct {
    wchar_t* wCharStr;
    size_t MaxCharCount;
    size_t CharsWritten;
} __wOutStrCtrl;

void* __FileWrite(void* file, const char* buffer, size_t char_num);
void* __StringWrite(void* ctrl_arg, const char* buffer, size_t char_num);
void* __wStringWrite(void* ctrl_arg, const wchar_t* buffer, size_t char_num);

#ifdef __cplusplus
}
#endif

#endif
