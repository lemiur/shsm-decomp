// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_ClumpSwapList.h>
#include <climax/Memory.h>

unk_ClumpSwapList* lbl_805297D0;

/*
====================
fn_8009EFA0
====================
*/
// 0x8009EFA0
extern "C" void fn_8009EFA0() {
    lbl_805297D0 = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpSwapList();
}

/*
====================
unk_ClumpSwapList::fn_8009EFE8

Files the clump under name, growing the array when it is full. Returns false when name is already
filed.

Open (wall: register/schedule and the grow path): retail keeps the search index in r29 and the byte
offset in r31, the reverse of this build, and its new[] has a zero-count test this build does not
emit.
====================
*/
// 0x8009EFE8
bool unk_ClumpSwapList::fn_8009EFE8( unk_RpClump* clump, const char* name ) {
    if ( IndexOf( name ) < 0 ) {
        unk_ClumpSwapEntry* entry = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpSwapEntry;
        if ( entry != NULL ) {
            entry->clump = clump;
            entry->name = fn_801B67FC( name, 0 );
        }

        u32 newCount = Count() + 1;
        if ( Capacity() < newCount ) {
            unk_ClumpSwapEntry** array = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpSwapEntry*[ newCount ];

            unk_ClumpSwapEntry** dst = array;
            for ( unk_ClumpSwapEntry** src = begin; src != end; ) {
                new( dst++ ) unk_ClumpSwapEntry*( *src++ );
            }
            for ( unk_ClumpSwapEntry** p = begin; p != end; p++ ) {
            }

            capacity = array + newCount;
            end = array + ( end - begin );
            if ( begin != NULL ) {
                delete[] begin;
            }
            begin = array;
        }

        new( end++ ) unk_ClumpSwapEntry*( entry );
        return true;
    }
    return false;
}

/*
====================
unk_ClumpSwapList::fn_8009F1A0

Drops the swap filed under name and frees its entry. Returns false when none is filed.

Open (wall: register/schedule): IndexOf's index and byte offset are in swapped registers.
====================
*/
// 0x8009F1A0
bool unk_ClumpSwapList::fn_8009F1A0( const char* name ) {
    int index = IndexOf( name );
    if ( index >= 0 ) {
        unk_ClumpSwapEntry* entry = begin[index];
        begin[index] = end[-1];
        end--;
        if ( entry != NULL ) {
            entry->clump = NULL;
            if ( entry->name != NULL ) {
                delete entry->name;
                entry->name = NULL;
            }
            delete entry;
        }
        return true;
    }
    return false;
}

/*
====================
unk_ClumpSwapList::fn_8009F298

The clump filed under name, or NULL when none is filed.

Open (wall: register/schedule): IndexOf's index and byte offset are in swapped registers.
====================
*/
// 0x8009F298
unk_RpClump* unk_ClumpSwapList::fn_8009F298( const char* name ) {
    int index = IndexOf( name );
    if ( index >= 0 ) {
        return begin[index]->clump;
    }
    return NULL;
}
