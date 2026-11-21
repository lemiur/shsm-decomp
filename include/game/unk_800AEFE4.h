#ifndef GAME_UNK_800AEFE4_H
#define GAME_UNK_800AEFE4_H

#include <revolution/types.h>

/*
 * Owner key for cache requests that belong to no spawner: fn_800AEFE4 and the requests beside it
 * pass its address as the owner, and fn_801B05C0 does not read it. Eight bytes of zeroed storage;
 * the contents are never read. Name invented.
 */
extern u8 lbl_80529888[8];

extern "C" {
void*   fn_800AEFE4( const char* name, int unused, int flags );
}

#endif
