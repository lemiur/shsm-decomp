// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800ABBDC.h>
#include <game/unk_Registry.h>
#include <extras.h>
#include <string.h>

/*
====================
fn_800ABBDC

Removes the entry called name from the registry and frees it. Returns 0, or -1 when no
entry has that name.

Not exact: the two search loops are laid out with the found exit as a bne/b pair where this
source's break emits a beq (control-flow shape); the rest of the body matches.
====================
*/
// 0x800ABBDC
extern "C" int fn_800ABBDC( const char* name ) {
    unk_RegistryEntry* entry;
    unk_RegistryNode* node;
    void* handle;

    for ( node = lbl_8046EA70.head; node != NULL; node = node->next ) {
        entry = node->data;
        if ( stricmp( entry->name, name ) == 0 ) {
            break;
        }
    }
    if ( node == NULL ) {
        entry = NULL;
    }
    if ( entry == NULL ) {
        return -1;
    }

    for ( node = lbl_8046EA70.head; node != NULL; node = node->next ) {
        if ( node->data == entry ) {
            break;
        }
    }
    if ( node != NULL ) {
        if ( node->prev == NULL ) {
            lbl_8046EA70.head = node->next;
        } else {
            node->prev->next = node->next;
        }
        if ( node->next == NULL ) {
            lbl_8046EA70.tail = node->prev;
        } else {
            node->next->prev = node->prev;
        }
        node->prev = NULL;
        node->next = NULL;
        lbl_8046EA70.count--;
        delete node;
    }

    entry->handle = NULL;
    entry->name[ 0 ] = '\0';
    if ( entry != NULL ) {
        handle = entry->handle;
        if ( handle != NULL && strlen( entry->name ) != 0 ) {
            entry->release( entry, handle );
        }
        delete entry;
    }
    return 0;
}
