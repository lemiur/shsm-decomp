// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AF100.h>
#include <climax/Utils.h>

/*
====================
fn_800AF100

Reads up to size bytes from the stream into dst, then byte-swaps every word read, because the
stored data is in the opposite byte order. Returns the byte count the read delivered.
====================
*/
// 0x800AF100
extern "C" u32 fn_800AF100( unk_AsyncStream* stream, u32* dst, u32 size ) {
    u32 bytesRead = fn_801B10AC( lbl_8052A5C0, stream, dst, size );
    u32 count = bytesRead >> 2;
    for ( u32 i = 0; i < count; i++ ) {
        dst[i] = fn_801B6C78( dst[i] );
    }
    return bytesRead;
}
