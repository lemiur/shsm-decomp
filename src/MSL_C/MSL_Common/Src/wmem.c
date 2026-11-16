// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MSL_C/MSL_Common/wmem.c
/* Unit name unconfirmed: MSL wmem.c, as in the public decomps; no DOL string names the file. */

#include <string.h>
#include <wchar.h>

/*
 * wmemcpy
 *
 * Copies n wide characters from src to dest.
 */
wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t n) {
    return (wchar_t*)memcpy(dest, src, n * sizeof(wchar_t));
} // 0x8019F7C0

/*
 * wmemchr
 *
 * Finds the first of the n wide characters at s equal to c; NULL when none is.
 * The count is an int, as in retail (signed test of n), not the standard size_t.
 */
wchar_t* wmemchr(const wchar_t* s, wchar_t c, int n) {
    int i;

    for (i = 0; i != n; s++, i++) {
        if (*s == c) {
            return (wchar_t*)s;
        }
    }

    return NULL;
} // 0x8019F7C8
