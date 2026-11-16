/*
 * fmod_stringw.cpp: FMOD's wide-character (UTF-16) string routines, used for unicode file names.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_stringw.obj)

#include <fmod/fmod_string.h>

/*
====================
FMOD_strlenW

The length of a wide string in characters.
====================
*/
int FMOD_strlenW( const short *string ) {
    const short *sc;

    for ( sc = string; *sc != 0; ++sc ) {
    }

    return sc - string;
} // 0x80376F2C

/*
====================
FMOD_strncpyW

Copies at most count characters of src, stopping after the terminator. Unlike strncpy it does not
pad the rest of dest with zeros.
====================
*/
short *FMOD_strncpyW( short *dest, const short *src, int count ) {
    short *tmp = dest;

    // RULE-EXCEPTION: R35, the C string-copy idiom; the bytes test the copied character in the loop condition
    while ( count-- && ( *dest++ = *src++ ) ) {
    }

    return tmp;
} // 0x80376F58

/*
====================
FMOD_wtoa

Narrows a wide string in place and returns it. It keeps the first byte of each character, which is
the high byte on this big-endian target (the original's bug, kept).
====================
*/
char *FMOD_wtoa( char *string ) {
    char *from;
    char *to;

    if ( !string ) {
        return 0;
    }

    from = string;
    to   = string;

    while ( *from ) {
        *to++ = *from;
        from += 2;
    }

    *to = 0;

    return string;
} // 0x80376F88
