/*
 * FMOD's own C string routines (fmod_string.cpp, with the wide-character ones in fmod_stringw.cpp):
 * FMOD does not use the C library's. They have C linkage; parameter and return types are taken from
 * their uses in retail.
 */
#ifndef _FMOD_STRING_H
#define _FMOD_STRING_H

extern "C" {

// ---- fmod_string.cpp ----
int    FMOD_strlen( const char *string );
char  *FMOD_strcpy( char *dest, const char *src );
char  *FMOD_strncpy( char *dest, const char *src, int count );
char  *FMOD_strcat( char *dest, const char *src );
char  *FMOD_strncat( char *dest, const char *src, int count );
char   FMOD_tolower( char c ); // no copy in retail (dead-stripped or inlined)
char  *FMOD_strupr( char *string );
char  *FMOD_strlwr( char *string );
// Case-sensitive comparison, 0 when equal.
int    FMOD_strcmp( const char *string1, const char *string2 );
int    FMOD_strncmp( const char *string1, const char *string2, int n );
// Case-insensitive comparison, 0 when equal (fmod_eventgroupi.cpp lookups).
int    FMOD_stricmp( const char *string1, const char *string2 );
int    FMOD_strnicmp( const char *string1, const char *string2, int count );
// Byte comparison, 0 when equal. The count is signed: retail tests it with a signed compare.
int    FMOD_memcmp( const void *buf1, const void *buf2, int count );
void  *FMOD_memmove( void *dest, const void *src, unsigned int count );
// A copy of the string in a fresh allocation, NULL when out of memory.
char  *FMOD_strdup( const char *string );
int    FMOD_atoi_hex( const char *string );

// ---- fmod_stringw.cpp ----
int    FMOD_strlenW( const short *string );
short *FMOD_strncpyW( short *dest, const short *src, int count );
char  *FMOD_wtoa( char *string );

}

#endif /* _FMOD_STRING_H */
