// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <climax/AttributeHandler.h>
#include <extras.h>
#include <game/unk_8009C88C.h>
#include <string.h>

/*
 * Reports whether the packet's first class-name command names a class other than name.
 * Walks the commands of the packet addressed to the class lbl_805297A8 until one with
 * id 0 carries a string; a non-empty string that stricmp does not equal to name answers true.
 * Names invented.
 */
// 0x8009C88C
bool fn_8009C88C( const char* name, const unk_AttributePacket& packet ) {
    bool differs = false;

    if ( name != NULL ) {
        const char* className = NULL;
        unk_AttributeCommandIterator it( packet, lbl_805297A8 );

        while ( className == NULL && !it.IsFinished() ) {
            if ( it->id == 0 ) {
                className = it->GetString();
            }
            ++it;
        }
        if ( className != NULL && strlen( className ) != 0 && stricmp( name, className ) != 0 ) {
            differs = true;
        }
    }
    return differs;
}
