// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80011AA4.h>
#include <climax/Heap.h>
#include <rwsdk/badevice.h>

/*
====================
fn_80011AA4

Reads a chunk of chunkSize bytes from the stream into a temporary buffer, then builds the
item from the buffer past its first eight bytes.

Open: the match is 90.96%; the wall is register/schedule only (the callee-saved numbering of
the three locals and the position of the parameter copy in the prologue). Control flow is
isomorphic, so the residue is the register permutation.
====================
*/
// 0x80011AA4
unk_80011AA4* fn_80011AA4( RwStream* stream, RwUInt32 chunkSize ) {
    unk_80011AA4* record = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_80011AA4;
    record->tag = 0x4D494E41;   /* "MINA" */

    void* buffer = RwMalloc( chunkSize, 0 );
    RwStreamRead( stream, buffer, chunkSize - 12 );

    /* unk_Item is 0x18 bytes */
    unk_Item* item = (unk_Item*)operator new( 0x18, GAME_HEAP, GAME_HEAP_FLAGS );
    if ( item != NULL ) {
        item = fn_80187458( item );
    }
    fn_80187504( item, (char*)buffer + 8 );
    record->item = item;

    operator delete( buffer );
    return record;
}

/*
====================
fn_80011B74

Destroys the record's item, then the record.
====================
*/
// 0x80011B74
void fn_80011B74( unk_80011AA4* record ) {
    fn_80187470( record->item, 1 );
    operator delete( record );
}
