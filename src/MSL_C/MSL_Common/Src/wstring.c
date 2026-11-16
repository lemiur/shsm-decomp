/* Unit name unconfirmed: wide-string counterparts of string.c; the filename is a guess. */
#include <wchar.h>

/*
 * wcslen
 *
 * Length of a wide string, terminator not counted.
 */
size_t wcslen(const wchar_t* str) {
    size_t len = -1;
    const wchar_t* p = str - 1;

    do {
        len++;
    } while (*++p);

    return len;
} // 0x801A1B08

/*
 * wcscpy
 *
 * Copies a wide string including its terminator.
 */
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src) {
    const wchar_t* p = src - 1;
    wchar_t* q = dst - 1;

    while ((*++q = *++p) != 0) {
    }

    return dst;
} // 0x801A1B24

/*
 * wcsncpy
 *
 * Copies at most n wide characters, padding the rest of dst with zeros.
 */
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t n) {
    const wchar_t* p = src - 1;
    wchar_t* q = dst - 1;

    n++;

    while (--n) {
        if (!(*++q = *++p)) {
            while (--n) {
                *++q = 0;
            }
            break;
        }
    }

    return dst;
} // 0x801A1B40

/*
 * wcsncat
 *
 * Appends at most n wide characters of src and always terminates the result.
 */
wchar_t* wcsncat(wchar_t* dst, const wchar_t* src, size_t n) {
    const wchar_t* p = src - 1;
    wchar_t* q = dst - 1;

    while (*++q) {
    }

    q--;
    n++;

    while (--n) {
        if (!(*++q = *++p)) {
            q--;
            break;
        }
    }

    *++q = 0;

    return dst;
} // 0x801A1B84

/*
 * wcscmp
 *
 * Difference of the first pair of wide characters that differ.
 */
int wcscmp(const wchar_t* str1, const wchar_t* str2) {
    const wchar_t* p1 = str1 - 1;
    const wchar_t* p2 = str2 - 1;
    wchar_t c1, c2;

    while ((c1 = *++p1) == (c2 = *++p2)) {
        if (!c1) {
            return 0;
        }
    }

    return c1 - c2;
} // 0x801A1BD0

/*
 * wcschr
 *
 * Finds the first occurrence of a wide character, the terminator included.
 */
wchar_t* wcschr(const wchar_t* str, wchar_t chr) {
    const wchar_t* p = str - 1;
    wchar_t ch;

    while ((ch = *++p) != 0) {
        if (ch == chr) {
            return (wchar_t*)p;
        }
    }

    return chr ? NULL : (wchar_t*)p;
} // 0x801A1C04
