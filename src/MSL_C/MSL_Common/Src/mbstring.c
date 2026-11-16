/* Unit name unconfirmed: multibyte conversions, MSL mbstring.c by recall; the filename is a guess. */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <MSL_C/MSL_Common/locale.h>
#include <MSL_C/MSL_Common/mbstring.h>

/*
 * mbtowc
 *
 * Decodes one multibyte character through the current locale.
 */
int mbtowc(wchar_t* pwc, const char* s, size_t n) {
    return _current_locale.ctype_cmpt_ptr->decode_mb(pwc, s, n);
} // 0x80199348

/*
 * __mbtowc_noconv
 *
 * The "C" locale's decoder: one byte per wide character.
 */
int __mbtowc_noconv(wchar_t* pwc, const char* s, size_t n) {
    if (!s) {
        return 0;
    }

    if (!n) {
        return -1;
    }

    if (pwc) {
        *pwc = (unsigned char)*s;
    }

    if (*s == 0) {
        return 0;
    }

    return 1;
} // 0x80199360

/*
 * __wctomb_noconv
 *
 * The "C" locale's encoder: the low byte of the wide character.
 */
int __wctomb_noconv(char* s, wchar_t wchar) {
    if (!s) {
        return 0;
    }

    *s = (char)wchar;
    return 1;
} // 0x801993AC

/*
 * mbstowcs
 *
 * Converts a multibyte string; the result is the number of wide characters
 * stored, or -1 on an invalid sequence.
 */
size_t mbstowcs(wchar_t* pwcs, const char* s, size_t n) {
    int i, count;
    size_t source_len = strlen(s);

    if (pwcs) {
        for (i = 0; i < n; i++) {
            if (*s) {
                count = mbtowc(pwcs++, s, source_len);

                if (count > 0) {
                    s += count;
                    source_len -= count;
                } else {
                    return -1;
                }
            } else {
                *pwcs = 0;
                break;
            }
        }
    } else {
        i = 0;
    }

    return i;
} // 0x801993C8

/*
 * wcstombs
 *
 * Converts a wide string, stopping before a character that would not fit
 * in n bytes. The encoder is called through the locale directly.
 */
size_t wcstombs(char* s, const wchar_t* pwcs, size_t n) {
    int result;
    int chars_written = 0;
    char temp[MB_LEN_MAX];

    const wchar_t* pwc;

    if (!s || !pwcs) {
        return 0;
    }

    pwc = pwcs;

    while (chars_written <= n) {
        if (!*pwc) {
            s[chars_written] = 0;
            break;
        }

        result = _current_locale.ctype_cmpt_ptr->encode_wc(temp, *pwc++);

        if (chars_written + result > n) {
            break;
        }

        strncpy(s + chars_written, temp, result);
        chars_written += result;
    }

    return chars_written;
} // 0x80199488
