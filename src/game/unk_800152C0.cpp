// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800152C0.h>
#include <extras.h>

/*
====================
fn_800152C0
====================
*/
// 0x800152C0
SClassInfo* fn_800152C0( const char* name ) {
    int i;

    for ( i = 0; i < lbl_805291C0; i++ ) {
        if ( stricmp( name, lbl_805291BC[ i ]->name ) == 0 ) {
            return lbl_805291BC[ i ];
        }
    }
    return NULL;
}
