#ifndef GAME_LOG_H
#define GAME_LOG_H

/*
 * printf-style debug output; in this build the functions only take their arguments and
 * return. The behaviours pass NULL for their own name (the "[%s]" in their formats).
 * Header name invented.
 */

// defined in 0x8002438C and 0x800A65B0, units not yet reconstructed
void    fn_8002438C( const char* format, ... );
void    fn_800A65B0( const char* format, ... );

/*
 * Debug output with a level, compiled out in release. MWCC cannot inline a variadic function, so
 * each unit that calls it keeps a weak copy; the link keeps the first, CPreCache's (0x80028D0C,
 * between that unit's own functions, where MWCC emits the unit's out-of-line inline copies).
 */
inline void fn_80028D0C( int level, const char* format, ... ) {
}

#endif
