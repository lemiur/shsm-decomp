// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// The registry name is "PreCache" (lbl_803E9F50, lbl_803E9F5C); the ".IGCStream" strings
// (lbl_803E9ED0, lbl_803E9EDC) give the suffix. The class name is invented.

#include <game/unk_IGCStream.h>
#include <stdio.h>
#include <string.h>

u32 lbl_80529870 = fn_801A7A84( "PreCache" );
unk_801A8F6C lbl_80529874( "PreCache", unk_IGCStream::fn_800AB6C8, sizeof( unk_IGCStream ) );

/*
====================
unk_IGCStream::~unk_IGCStream
====================
*/
// 0x800AB544
unk_IGCStream::~unk_IGCStream() {
    delete streamName;
}

/*
====================
unk_IGCStream::fn_80028BB0

Gives an asset name that has no extension the ".IGCStream" suffix, in a buffer the object owns,
and names the asset by that buffer.
====================
*/
// 0x800AB5C4
void unk_IGCStream::fn_80028BB0() {
    if ( name != NULL && strstr( name, "." ) == NULL ) {
        int length = strlen( name ) + strlen( ".IGCStream" ) + 1;
        streamName = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[ length ];
        sprintf( streamName, "%s.IGCStream", name );
        name = streamName;
    }
}
