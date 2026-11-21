// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80074A08.h>
#include <climax/Memory.h>

// Clamps a request's slot index to the range its table allows. Name invented.
inline const int& unk_ClampIndex( const int& value, const int& low, const int& high ) {
    if ( value > high ) {
        return high;
    }
    if ( value < low ) {
        return low;
    }
    return value;
}

// Appends a node holding the entry's clump to the clump list and gives the clump up to the list.
inline void AppendClump( unk_ClumpRoster* roster, unk_ClumpEntry* entry ) {
    if ( entry != NULL ) {
        unk_RpClump* clump = entry->clump;
        if ( clump == NULL ) {
            return;
        }
        unk_ClumpNode* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpNode( clump );
        node->prev = roster->tail;
        node->next = NULL;
        if ( roster->tail != NULL ) {
            roster->tail->next = node;
        }
        if ( roster->head == NULL ) {
            roster->head = node;
        }
        roster->tail = node;
        roster->count++;
        entry->clump = NULL;
    }
}

/*
====================
fn_800751A8

Files a clump for the entry: a clone of the slot's clump when both slot indices name the same
slot, otherwise the clump fn_80073FB8 builds from the two slots.

Not exact (7 rows): the compiler keeps the address of the first key and the flag constant in the
opposite registers (register wall; the declaration and initialiser orders tried did not move it).
====================
*/
// 0x800751A8
void fn_800751A8( unk_ClumpRoster* roster, unk_ClumpEntry* entry ) {
    if ( entry != NULL && entry->clump == NULL ) {
        unk_RpClump* clump;
        if ( entry->slotA == entry->slotB ) {
            clump = RpClumpClone( roster->slots[ entry->slotB ].clump );
        } else {
            int first = 9;
            int second = 10;
            unk_ClumpPart parts[ 2 ];
            parts[ 0 ].clump = roster->slots[ entry->slotB ].clump;
            parts[ 0 ].key = &first;
            parts[ 0 ].flag = 1;
            parts[ 1 ].clump = roster->slots[ entry->slotA ].clump;
            parts[ 1 ].key = &second;
            parts[ 1 ].flag = 1;
            clump = fn_80073FB8( parts, 2, roster->slots[ entry->slotA ].clump, 1 );
        }
        entry->clump = clump;
    }
}

/*
====================
fn_80075264

Appends a node holding the entry's clump to the clump list and gives the clump up to the list.
====================
*/
// 0x80075264
void fn_80075264( unk_ClumpRoster* roster, unk_ClumpEntry* entry ) {
    AppendClump( roster, entry );
}

/*
====================
fn_80075324

Walks the clump list from its tail. Each clump is taken out of its world when removeFromWorld is
set, and a clump that is in no world is destroyed and its node unlinked and freed.
====================
*/
// 0x80075324
void fn_80075324( unk_ClumpRoster* roster, int removeFromWorld ) {
    unk_ClumpNode* node = roster->tail;
    while ( node != NULL ) {
        unk_ClumpNode* current;
        unk_RpClump* clump = node->clump;
        current = node;
        node = node->prev;
        if ( clump == NULL ) {
            continue;
        }
        if ( removeFromWorld != 0 && RpClumpGetWorld( clump ) != NULL ) {
            RpWorldRemoveClump( RpClumpGetWorld( clump ), clump );
        }
        if ( RpClumpGetWorld( clump ) == NULL ) {
            if ( clump != NULL ) {
                RpClumpDestroy( clump );
            }
            if ( current != NULL ) {
                if ( current->prev == NULL ) {
                    roster->head = current->next;
                } else {
                    current->prev->next = current->next;
                }
                if ( current->next == NULL ) {
                    roster->tail = current->prev;
                } else {
                    current->next->prev = current->prev;
                }
                current->prev = NULL;
                current->next = NULL;
                roster->count--;
                delete current;
            }
        }
    }
}

/*
====================
fn_80074A08

Rebuilds the roster from its source record. Each flag of the record requests a slot, by its name's
index into the first or the second request list; the slots are then loaded or released by their
requests. The request list is filed into the clump list, and the requests are combined into a
random order of new requests.

Not exact: register wall in the loop that files the requests (the compiler keeps the two list
counts in the opposite registers).
====================
*/
// 0x80074A08
void fn_80074A08( unk_ClumpRoster* roster ) {
    int count1 = 0;
    int count2 = 0;
    int list1[ 3 ] = { 0, 0, 0 };
    int list2[ 3 ] = { 0, 0, 0 };
    int total = 0;
    int placed = 0;
    bool hasZero1 = false;
    bool hasZero2 = false;

    for ( int i = 0; i < 9; i++ ) {
        roster->slots[ i ].active = 0;
    }
    if ( roster->field_0x0 > 0 ) {
        if ( roster->source->flag0 != 0 ) {
            list1[ 0 ] = 0;
            hasZero1 = true;
            count1 = 1;
            roster->slots[ 0 ].active = 1;
        }
        if ( roster->source->flag2 != 0 && roster->source->name2 != NULL && roster->source->name2[ 0 ] != '\0' ) {
            int index = 0;
            if ( fn_8014F114( lbl_8052A0D0, roster->source->name2, &index ) ) {
                int value = index + 1;
                int slot = unk_ClampIndex( value, lbl_80527068, lbl_8052706C );
                list1[ count1++ ] = slot;
                roster->slots[ slot ].active = 1;
            } else if ( !hasZero1 ) {
                list1[ count1++ ] = 0;
                hasZero1 = true;
                roster->slots[ 0 ].active = 1;
            }
        }
        if ( roster->source->flag4 != 0 && roster->source->name4 != NULL && roster->source->name4[ 0 ] != '\0' ) {
            int index = 0;
            if ( fn_8014F114( lbl_8052A0D0, roster->source->name4, &index ) ) {
                int value = index + 5;
                int slot = unk_ClampIndex( value, lbl_80527070, lbl_80527074 );
                list1[ count1++ ] = slot;
                roster->slots[ slot ].active = 1;
            } else if ( !hasZero1 ) {
                list1[ count1++ ] = 0;
                hasZero1 = true;
                roster->slots[ 0 ].active = 1;
            }
        }
        if ( roster->source->flag1 != 0 ) {
            list2[ 0 ] = 0;
            hasZero2 = true;
            count2 = 1;
            roster->slots[ 0 ].active = 1;
        }
        if ( roster->source->flag3 != 0 && roster->source->name3 != NULL && roster->source->name3[ 0 ] != '\0' ) {
            int index = 0;
            if ( fn_8014F114( lbl_8052A0D0, roster->source->name3, &index ) ) {
                int value = index + 1;
                int slot = unk_ClampIndex( value, lbl_80527078, lbl_8052707C );
                list2[ count2++ ] = slot;
                roster->slots[ slot ].active = 1;
            } else if ( !hasZero2 ) {
                list2[ count2++ ] = 0;
                hasZero2 = true;
                roster->slots[ 0 ].active = 1;
            }
        }
        if ( roster->source->flag5 != 0 && roster->source->name5 != NULL && roster->source->name5[ 0 ] != '\0' ) {
            int index = 0;
            if ( fn_8014F114( lbl_8052A0D0, roster->source->name5, &index ) ) {
                int value = index + 5;
                int slot = unk_ClampIndex( value, lbl_80527080, lbl_80527084 );
                list2[ count2++ ] = slot;
                roster->slots[ slot ].active = 1;
            } else if ( !hasZero2 ) {
                list2[ count2++ ] = 0;
                hasZero2 = true;
                roster->slots[ 0 ].active = 1;
            }
        }
    }

    roster->loadedCount = 0;
    for ( int i = 0; i < 9; i++ ) {
        unk_ClumpSlot* slot = &roster->slots[ i ];
        if ( slot->active != 0 && slot->assets == NULL && slot->loaded == 0 ) {
            slot->resource = fn_801B05C0( lbl_8052A5C0, lbl_803E6568[ i ], roster, 0, NULL, NULL );
            if ( slot->resource != NULL ) {
                slot->loaded = 1;
                roster->loadedCount++;
            }
        } else if ( slot->active == 0 ) {
            if ( slot->assets != NULL ) {
                slot->assets->DecRefCount();
                slot->assets = NULL;
                slot->clump = NULL;
            } else if ( slot->loaded != 0 && slot->resource != NULL ) {
                fn_801B0B8C( lbl_8052A5C0, slot->resource, roster, 1 );
                slot->resource = NULL;
                slot->loaded = 0;
            }
        }
    }

    if ( roster->field_0xC > 0 ) {
        while ( roster->field_0x10 != NULL ) {
            unk_ClumpEntry* entry = roster->field_0x10->entry;
            if ( entry->field_0x0 != 0 ) {
                AppendClump( roster, entry );
                entry->field_0x0 = 0;
            }
            delete entry;
            unk_ClumpEntryNode* node = roster->field_0x10;
            while ( node != NULL && node->entry != entry ) {
                node = node->next;
            }
            if ( node != NULL ) {
                if ( node->prev == NULL ) {
                    roster->field_0x10 = node->next;
                } else {
                    node->prev->next = node->next;
                }
                if ( node->next == NULL ) {
                    roster->field_0x14 = node->prev;
                } else {
                    node->next->prev = node->prev;
                }
                node->prev = NULL;
                node->next = NULL;
                roster->field_0xC--;
                delete node;
            }
        }
    }

    if ( roster->field_0x0 > 0 ) {
        int period = count1 * count2;
        while ( total < roster->field_0x0 ) {
            total += period;
        }
        unk_ClumpEntry** entries = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpEntry*[ total ];
        while ( placed < total ) {
            for ( int a = 0; a < count1; a++ ) {
                for ( int b = 0; b < count2; b++ ) {
                    entries[ placed++ ] = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpEntry( list1[ a ], list2[ b ] );
                }
            }
        }

        int remaining = placed;
        for ( int i = 0; i < roster->field_0x0; i++ ) {
            int pick = RpRandom() % remaining;
            unk_ClumpEntryNode* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpEntryNode( entries[ pick ] );
            node->next = roster->field_0x10;
            node->prev = NULL;
            if ( roster->field_0x10 != NULL ) {
                roster->field_0x10->prev = node;
            }
            if ( roster->field_0x14 == NULL ) {
                roster->field_0x14 = node;
            }
            roster->field_0x10 = node;
            remaining--;
            roster->field_0xC++;
            if ( remaining > 0 && pick != remaining ) {
                entries[ pick ] = entries[ remaining ];
            }
        }
        delete[] entries;
    }
}
