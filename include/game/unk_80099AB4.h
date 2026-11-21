#ifndef GAME_UNK_80099AB4_H
#define GAME_UNK_80099AB4_H

#include <revolution/types.h>
#include <revolution/thp.h>

extern "C" {
/* Opens the movie at path and starts it, or closes it when it does not open; returns the decode buffer
 * (NULL after a failed open). Name invented: the unit is unnamed. */
u8*     fn_80099AB4( const char* path, u32 flags );

/* Callees, defined in their owning units (not yet reconstructed). */
void    fn_8009A730( s32 );
s32     fn_8009AA00( const char* path, s32 );
s32     fn_8009ACC8( void );
void    fn_8009AD80( u8* buffer );
void    fn_8009B37C( void );
}

extern THPVideoInfo    lbl_8046A790;
extern THPAudioInfo    lbl_8046A7A0;
extern u8*             lbl_80529768;

#endif
