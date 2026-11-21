#ifndef GAME_UNK_800AF188_H
#define GAME_UNK_800AF188_H

#include <revolution/types.h>

/*
 * Reads size bytes from a stream into buffer, then byte-swaps the shorts read. Returns the byte
 * count the read produced. Name invented; the stream type is not yet known.
 */
extern "C" {
u32 fn_800AF188( void* stream, s16* buffer, u32 size );
}

#endif
