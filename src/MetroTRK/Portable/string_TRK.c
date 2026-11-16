// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/debugger/embedded/MetroTRK/Portable/string_TRK.c

#include "TRK/string_TRK.h"

/* Counts the characters before the terminating NUL. Reads the bytes as unsigned, and p starts one byte
 * before str so that the pre-increment reads the first character. */
int TRK_strlen(const char* str) {
    const unsigned char* p = (const unsigned char*)str - 1;
    int n = -1;

    do {
        n++;
    } while (*++p != '\0');
    return n;
} // 0x80192164
