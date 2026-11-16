#ifndef _MSL_WCTYPE_H
#define _MSL_WCTYPE_H

#include <ctype.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * iswdigit
 *
 * Tests the digit class of the current locale's wide ctype map; anything
 * outside 0-255 is not a digit.
 */
static inline int iswdigit(wint_t c) {
    return (c < 0 || c >= 256) ? 0 : (_current_locale.ctype_cmpt_ptr->wctype_map_ptr[c] & __digit_char);
}

/*
 * iswupper
 *
 * Tests the upper-case class of the current locale's wide ctype map; anything
 * outside 0-255 is not upper case.
 */
static inline int iswupper(wint_t c) {
    return (c < 0 || c >= 256) ? 0 : (_current_locale.ctype_cmpt_ptr->wctype_map_ptr[c] & __upper_char);
}

#ifdef __cplusplus
}
#endif

#endif
