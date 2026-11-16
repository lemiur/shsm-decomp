// Unit name and boundary: Gormiti link map (fmodex.a fmod_string.obj)

#include <fmod/fmod_memory.h>
#include <fmod/fmod_string.h>

/*
====================
FMOD_strlen

Returns the number of characters before the terminator.
====================
*/
int FMOD_strlen( const char *string ) {
    char *sc;

    for ( sc = (char *)string; *sc != '\0'; sc++ )
        ;
    return sc - string;
} // 0x80376A18

/*
====================
FMOD_strcpy

Copies src, terminator included, into dest and returns dest.
====================
*/
char *FMOD_strcpy( char *dest, const char *src ) {
    char *tmp = dest;

    while ( ( *dest++ = *src++ ) != '\0' )
        ;
    return tmp;
} // 0x80376A38

/*
====================
FMOD_strncpy

Copies at most count characters; the rest of dest is left alone once the source ends or count runs out.
====================
*/
char *FMOD_strncpy( char *dest, const char *src, int count ) {
    char *tmp = dest;

    while ( count-- && ( *dest++ = *src++ ) != '\0' )
        ;
    return tmp;
} // 0x80376A5C

/*
====================
FMOD_strcat

Appends src to the end of dest and returns dest.
====================
*/
char *FMOD_strcat( char *dest, const char *src ) {
    char *tmp = dest;

    while ( *dest != '\0' )
        dest++;
    while ( ( *dest++ = *src++ ) != '\0' )
        ;
    return tmp;
} // 0x80376A8C

/*
====================
FMOD_strncat

Appends at most count characters of src to dest, always terminating the result.
====================
*/
char *FMOD_strncat( char *dest, const char *src, int count ) {
    char *tmp = dest;

    if ( count != 0 ) {
        while ( *dest != '\0' )
            dest++;
        while ( ( *dest++ = *src++ ) != '\0' ) {
            if ( --count == 0 ) {
                *dest = '\0';
                break;
            }
        }
    }
    return tmp;
} // 0x80376AC4

/*
====================
FMOD_tolower

Lower-cases an ASCII capital letter; any other character passes through. FMOD_stricmp and FMOD_strnicmp
inline it; its out-of-line copy is dead-stripped in retail (Gormiti map: UNUSED, 0x1c bytes).
====================
*/
char FMOD_tolower( char c ) {
    if ( c >= 'A' && c <= 'Z' ) {
        c += 'a' - 'A';
    }
    return c;
} // no copy in retail (dead-stripped or inlined)

/*
====================
FMOD_strupr

Upper-cases the string in place and returns it.
====================
*/
char *FMOD_strupr( char *string ) {
    char *cp;

    for ( cp = string; *cp != '\0'; cp++ ) {
        if ( *cp >= 'a' && *cp <= 'z' ) {
            *cp -= 'a' - 'A';
        }
    }
    return string;
} // 0x80376B1C

/*
====================
FMOD_strlwr

Lower-cases the string in place and returns it.
====================
*/
char *FMOD_strlwr( char *string ) {
    char *cp;

    for ( cp = string; *cp != '\0'; cp++ ) {
        if ( *cp >= 'A' && *cp <= 'Z' ) {
            *cp += 'a' - 'A';
        }
    }
    return string;
} // 0x80376B58

/*
====================
FMOD_strcmp

Case-sensitive comparison: the difference of the first characters that differ, 0 when equal.
====================
*/
int FMOD_strcmp( const char *string1, const char *string2 ) {
    char c1;
    char c2;

    do {
        c1 = *string1++;
        c2 = *string2++;
    } while ( c1 != '\0' && c1 == c2 );

    return c1 - c2;
} // 0x80376B94

/*
====================
FMOD_strncmp

As FMOD_strcmp, over at most n characters. The parameter name is invented: the DWARF records no parameters.
====================
*/
int FMOD_strncmp( const char *string1, const char *string2, int n ) {
    char c1;
    char c2;
    int  count = 0;

    do {
        c1 = *string1++;
        c2 = *string2++;
        count++;
    } while ( c1 != '\0' && c1 == c2 && count < n );

    return c1 - c2;
} // 0x80376BC0

/*
====================
FMOD_stricmp

Case-insensitive FMOD_strcmp.
====================
*/
int FMOD_stricmp( const char *string1, const char *string2 ) {
    char c1;
    char c2;

    do {
        c1 = FMOD_tolower( *string1++ );
        c2 = FMOD_tolower( *string2++ );
    } while ( c1 != '\0' && c1 == c2 );

    return c1 - c2;
} // 0x80376BFC

/*
====================
FMOD_strnicmp

Case-insensitive FMOD_strncmp. The parameter name is invented: the DWARF records no parameters.
====================
*/
int FMOD_strnicmp( const char *string1, const char *string2, int n ) {
    char c1;
    char c2;
    int  count = 0;

    do {
        c1 = FMOD_tolower( *string1++ );
        c2 = FMOD_tolower( *string2++ );
        count++;
    } while ( c1 != '\0' && c1 == c2 && count < n );

    return c1 - c2;
} // 0x80376C5C

/*
====================
FMOD_memcmp

Byte comparison of count bytes, 0 when equal. The count is an int: the retail loop tests it with a
signed compare.
====================
*/
int FMOD_memcmp( const void *buf1, const void *buf2, int count ) {
    unsigned char *su1 = (unsigned char *)buf1;
    unsigned char *su2 = (unsigned char *)buf2;
    signed char    res = 0;

    for ( ; count > 0; count-- ) {
        res = *su1 - *su2;
        if ( res != 0 ) {
            break;
        }
        su1++;
        su2++;
    }
    return res;
} // 0x80376CCC

/*
====================
FMOD_memmove

Copies count bytes, correct for overlapping buffers. The compiler unrolls each byte loop by eight (CTR loops).
====================
*/
void *FMOD_memmove( void *dest, const void *src, unsigned int count ) {
    char *tmp;
    char *s;

    if ( dest <= src ) {
        tmp = (char *)dest;
        s = (char *)src;
        while ( count-- ) {
            *tmp++ = *s++;
        }
    } else {
        tmp = (char *)dest + count;
        s = (char *)src + count;
        while ( count-- ) {
            *--tmp = *--s;
        }
    }
    return dest;
} // 0x80376D04

/*
====================
FMOD_strdup

A copy of the string in a fresh allocation, NULL when out of memory.
====================
*/
char *FMOD_strdup( const char *string ) {
    char *ret;

#line 239
    ret = (char *)FMOD_Memory_Alloc( FMOD_strlen( string ) + 1 );
    if ( ret != NULL ) {
        FMOD_strcpy( ret, string );
    }
    return ret;
} // 0x80376E10

/*
====================
FMOD_atoi_hex

Reads hexadecimal digits from the start of the string.
====================
*/
int FMOD_atoi_hex( const char *string ) {
    int val;

    if ( string == NULL ) {
        return 0;
    }
    val = 0;
    for ( const char *p = string; *p != '\0'; p++ ) {
        val <<= 4;
        if ( *p >= '0' && *p <= '9' ) {
            val = val + *p - '0';
        } else if ( *p >= 'A' && *p <= 'F' ) {
            val = val + *p - 'A' + 10;
        } else if ( *p >= 'a' && *p <= 'f' ) {
            val = val + *p - 'a' + 10;
        }
    }
    return val;
} // 0x80376E9C
