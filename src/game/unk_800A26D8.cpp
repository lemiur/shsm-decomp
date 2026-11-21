// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <climax/AttributeHandler.h>
#include <game/CSystemCommands.h>
#include <game/unk_800A26D8.h>

/*
====================
fn_800A26D8

Walks the system commands of the packet and returns the payload of the first one with id 0,
or NULL when the packet has none.
====================
*/
// 0x800A26D8
const char* fn_800A26D8( const unk_AttributePacket& packet ) {
    const char* name = NULL;
    for ( unk_AttributeCommandIterator it( packet, lbl_805297E8 ); !it.IsFinished() && name == NULL; ++it ) {
        if ( it->id == 0 ) {
            name = it->data;
        }
    }
    return name;
}
