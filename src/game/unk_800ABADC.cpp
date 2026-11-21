// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800ABADC.h>
#include <climax/Heap.h>
#include <string.h>

/*
 * Registers a named handle: a new entry takes a copy of the name, the handle and the release callback,
 * and a node for it is appended to the registry. Always returns 0.
 */
// 0x800ABADC
int fn_800ABADC( const char* name, void* handle, unk_RegistryReleaseFn release ) {
    unk_RegistryEntry* entry = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_RegistryEntry;
    if ( entry != NULL ) {
        strcpy( entry->name, name );
        entry->handle = handle;
        entry->release = release;
        entry->refCount = 0;
    }
    unk_RegistryNode* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_RegistryNode( entry );
    node->prev = lbl_8046EA70.tail;
    node->next = NULL;
    if ( lbl_8046EA70.tail != NULL ) {
        lbl_8046EA70.tail->next = node;
    }
    if ( lbl_8046EA70.head == NULL ) {
        lbl_8046EA70.head = node;
    }
    lbl_8046EA70.tail = node;
    lbl_8046EA70.count++;
    return 0;
}
