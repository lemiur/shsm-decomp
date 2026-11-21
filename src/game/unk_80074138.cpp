// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80074138.h>
#include <climax/Heap.h>
#include <string.h>

/*
====================
unk_80074138::MoveAToB
====================
*/
// Moves the pending items' clumps from the first list to the second (inline in the constructor
// and the destructor).
inline void unk_80074138::MoveAToB() {
    while ( listA.head != NULL ) {
        unk_RpClump* clump;
        unk_PendingItem* item = listA.head->data;
        if ( item->field_0x00 != 0 ) {
            clump = item->field_0x0C;
            if ( clump != NULL ) {
                unk_List< unk_RpClump* >::Node* moved =
                    new( 2, 0x40000 ) unk_List< unk_RpClump* >::Node( clump );
                moved->prev = listB.tail;
                moved->next = NULL;
                if ( listB.tail != NULL ) {
                    listB.tail->next = moved;
                }
                if ( listB.head == NULL ) {
                    listB.head = moved;
                }
                listB.tail = moved;
                listB.count++;
                item->field_0x0C = NULL;
            }
            item->field_0x00 = 0;
        }
        delete item;

        unk_List< unk_PendingItem* >::Node* node;
        for ( node = listA.head; node != NULL; node = node->next ) {
            if ( node->data == item ) {
                break;
            }
        }
        listA.Remove( node );
    }
}

/*
====================
fn_80074138
====================
*/
// Wall (register/schedule): the transfer loop keeps item and clump in the swapped registers and retail repeats its null tests.
// 0x80074138
unk_80074138::unk_80074138( int key, int value ) :
    field_0x00( 0 ),
    field_0x04( value ),
    listA(),
    listB(),
    field_0x100( 0 ) {
    memset( slots, 0, sizeof( slots ) );
    if ( field_0x00 != key ) {
        MoveAToB();
        field_0x00 = key;
        fn_80074A08( this );
    }
}

/*
====================
fn_800744B0
====================
*/
// Wall (control flow/register): the clump loop and the slot loop are in retail's order, but the transfer loop inside keeps the same register and test wall as the constructor.
// 0x800744B0
unk_80074138::~unk_80074138() {
    MoveAToB();

    unk_List< unk_RpClump* >::Node* node = listB.tail;
    while ( node != NULL ) {
        unk_RpClump* clump = node->data;
        unk_List< unk_RpClump* >::Node* current = node;
        node = node->prev;
        if ( clump != NULL ) {
            if ( RpClumpGetWorld( clump ) != NULL ) {
                RpWorldRemoveClump( RpClumpGetWorld( clump ), clump );
            }
            if ( RpClumpGetWorld( clump ) == NULL ) {
                RpClumpDestroy( clump );
                listB.Remove( current );
            }
        }
    }

    for ( int i = 0; i < 9; i++ ) {
        unk_SlotEntry& slot = slots[ i ];
        if ( slot.field_0x0C != NULL ) {
            slot.field_0x0C->DecRefCount();
            slot.field_0x0C = NULL;
            slot.field_0x10 = 0;
        }
        if ( slot.field_0x04 != NULL ) {
            fn_801B2004( lbl_8052A5C0, slot.field_0x04 );
            slot.field_0x04 = NULL;
        }
        if ( slot.field_0x08 != NULL ) {
            fn_801B0B8C( lbl_8052A5C0, slot.field_0x08, this, 1 );
            slot.field_0x08 = NULL;
            slot.field_0x01 = 0;
        }
    }
}
