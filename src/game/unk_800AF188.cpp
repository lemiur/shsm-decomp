// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AF188.h>
#include <game/unk_ResourceManager.h>
#include <climax/Utils.h>

/*
====================
fn_800AF188

Reads size bytes from the stream into buffer through the resource manager, then byte-swaps each
short read (the data is stored little-endian). Returns the number of bytes read.
====================
*/
// 0x800AF188
extern "C" u32 fn_800AF188( void* stream, s16* buffer, u32 size ) {
    u32 bytesRead = fn_801B10AC( lbl_8052A5C0, stream, buffer, size );
    u32 count = bytesRead >> 1;
    u32 i;

    for ( i = 0; i < count; i++ ) {
        buffer[i] = fn_801B6C98( buffer[i] );
    }
    return bytesRead;
}
