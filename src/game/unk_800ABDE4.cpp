// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800ABDE4.h>

/*
====================
fn_800ABDE4
====================
*/
// Not exact: control flow; the search loop's exit wiring differs from retail (wall-identifier).
// 0x800ABDE4
extern "C" void* fn_800ABDE4( const char* name ) {
    unk_RegistryEntry* item;
    unk_RegistryNode* node;

    for ( node = lbl_8046EA70.head; node != NULL; node = node->next ) {
        item = node->data;
        if ( stricmp( item->name, name ) == 0 ) {
            break;
        }
    }
    if ( node == NULL ) {
        item = NULL;
    }
    return item == NULL ? NULL : item->handle;
}
