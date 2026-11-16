#ifndef _MSL_CTYPE_H
#define _MSL_CTYPE_H

#include <MSL_C/MSL_Common/locale.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Class bits of the locale's ctype map. */
#define __alpha_char 0x0001
#define __digit_char 0x0008
#define __space_char 0x0100
#define __upper_char 0x0200
#define __hex_char 0x0400

/*
 * isspace
 *
 * Tests the space class of the current locale's ctype map; anything outside
 * 0-255 is not a space.
 */
static inline int isspace(int c) {
    return (c < 0 || c >= 256) ? 0 : (_current_locale.ctype_cmpt_ptr->ctype_map_ptr[c] & __space_char);
}

/*
 * isalpha
 *
 * Tests the alphabetic class of the current locale's ctype map; anything outside
 * 0-255 is not a letter.
 */
static inline int isalpha(int c) {
    return (c < 0 || c >= 256) ? 0 : (_current_locale.ctype_cmpt_ptr->ctype_map_ptr[c] & __alpha_char);
}

/*
 * isdigit
 *
 * Tests the digit class of the current locale's ctype map; anything outside
 * 0-255 is not a digit.
 */
static inline int isdigit(int c) {
    return (c < 0 || c >= 256) ? 0 : (_current_locale.ctype_cmpt_ptr->ctype_map_ptr[c] & __digit_char);
}

/*
 * isxdigit
 *
 * Tests the hexadecimal-digit class of the current locale's ctype map; anything
 * outside 0-255 is not a hexadecimal digit.
 */
static inline int isxdigit(int c) {
    return (c < 0 || c >= 256) ? 0 : (_current_locale.ctype_cmpt_ptr->ctype_map_ptr[c] & __hex_char);
}

/*
 * isupper
 *
 * Tests the upper-case class of the current locale's ctype map; anything outside
 * 0-255 is not upper case.
 */
static inline int isupper(int c) {
    return (c < 0 || c >= 256) ? 0 : (_current_locale.ctype_cmpt_ptr->ctype_map_ptr[c] & __upper_char);
}

/*
 * tolower
 *
 * Maps a character through the current locale's lower-case table; anything outside
 * 0-255 is returned unchanged.
 */
static inline int tolower(int c) {
    return (c < 0 || c >= 256) ? c : _current_locale.ctype_cmpt_ptr->lower_map_ptr[c];
}

/*
 * toupper
 *
 * Maps a character through the current locale's upper-case table; anything outside
 * 0-255 is returned unchanged.
 */
static inline int toupper(int c) {
    return (c < 0 || c >= 256) ? c : _current_locale.ctype_cmpt_ptr->upper_map_ptr[c];
}

#ifdef __cplusplus
}
#endif

#endif
