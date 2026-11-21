// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8002A9A4.h>
#include <climax/Heap.h>
#include <extras.h>
#include <rwsdk/plcore/babinary.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names): none. The 1.5f of fn_8002A9A4 is the compiler's own constant (.sdata2 0x8052B8B0).

// The constructor fn_8012BDB0 on game-heap storage, as a new-expression of the class calls it.
inline unk_8012BDB0* NewObj( float f1, unk_RefList* owner ) {
    unk_8012BDB0* obj = (unk_8012BDB0*)operator new( sizeof( unk_8012BDB0 ), GAME_HEAP, GAME_HEAP_FLAGS );
    if ( obj != NULL ) {
        obj = fn_8012BDB0( obj, 1, f1, 1.5f, owner );
    }
    return obj;
}

/*
====================
fn_8002A9A4
====================
*/
// Two leading arguments are ABI slots this function never reads (r3, r4). Wall: the list appends
// are written out in place (MWCC does not inline a function that takes a counted pointer by value),
// and the 1.5f operand is reloaded per call instead of being kept in a register.
// 0x8002A9A4
void fn_8002A9A4( void* unused0, s32 unused1, float f1, unk_RefList* first, unk_RefList* second ) {
    unk_Ref objA = NewObj( f1, second );
    {
        unk_Ref ref = objA;
        unk_RefNode* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_RefNode( ref.ptr );
        node->prev = first->tail;
        node->next = NULL;
        if ( first->tail != NULL ) {
            first->tail->next = node;
        }
        if ( first->head == NULL ) {
            first->head = node;
        }
        first->tail = node;
        first->count++;
    }
    unk_Ref objB = NewObj( f1, first );
    {
        unk_Ref ref = objB;
        unk_RefNode* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_RefNode( ref.ptr );
        node->prev = second->tail;
        node->next = NULL;
        if ( second->tail != NULL ) {
            second->tail->next = node;
        }
        if ( second->head == NULL ) {
            second->head = node;
        }
        second->tail = node;
        second->count++;
    }
    objA.ptr->partner = objB.ptr;
    objB.ptr->partner = objA.ptr;
}

/*
====================
fn_8002AD78
====================
*/
// 0x8002AD78
void fn_8002AD78( unk_RefList* list, void* unused, unk_8012BDB0** source ) {
    unk_RefNode* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_RefNode( *source );
    node->prev = list->tail;
    node->next = NULL;
    if ( list->tail != NULL ) {
        list->tail->next = node;
    }
    if ( list->head == NULL ) {
        list->head = node;
    }
    list->tail = node;
    list->count++;
}

/*
====================
fn_8002AE2C
====================
*/
// 0x8002AE2C
unk_NameCell* fn_8002AE2C( unk_NameIndex* table, const char* name ) {
    for ( s32 i = 0; i < table->header->count; i++ ) {
        unk_NameCell* cell = table->cells[i];
        RwUInt32 offset = ( (RwUInt32*)table->blob )[ cell->record->nameIndex ];
        RwMemNative32( &offset, 4 );
        if ( stricmp( table->blob + offset, name ) == 0 ) {
            return table->cells[i];
        }
    }
    return NULL;
}

/*
====================
fn_8002AEF0
====================
*/
// 0x8002AEF0
unk_NamedEntryRecord* fn_8002AEF0( unk_NamedEntryList* list, unk_NameBlob* names, const char* name ) {
    unk_NamedEntryRecord* found = NULL;
    for ( s32 i = 0; i < list->header->count; i++ ) {
        unk_NamedEntryRecord* entry = &list->entries[i];
        RwUInt32 offset = ( (RwUInt32*)names->blob )[ entry->nameIndex ];
        RwMemNative32( &offset, 4 );
        if ( stricmp( name, names->blob + offset ) == 0 ) {
            found = entry;
        }
    }
    return found;
}

