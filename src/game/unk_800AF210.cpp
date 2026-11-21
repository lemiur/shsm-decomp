// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800AF210.h>
#include <game/unk_ResourceManager.h>
#include <climax/Utils.h>

/*
====================
fn_800AF210

Reads a block into dst through the resource manager, then byte-swaps each float of it. Returns
the number of bytes read. elemSize is the element size the callers pass (4 for this float form);
it is the size argument of the read.
====================
*/
// 0x800AF210
extern "C" u32 fn_800AF210( void* source, float* dst, u32 elemSize ) {
    u32 bytes = fn_801B10AC( lbl_8052A5C0, source, dst, elemSize );
    u32 count = bytes >> 2;
    u32 i;

    for ( i = 0; i < count; i++ ) {
        dst[i] = fn_801B6CAC( dst[i] );
    }
    return bytes;
}
