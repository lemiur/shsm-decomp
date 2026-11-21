#ifndef GAME_UNK_803E0880_H
#define GAME_UNK_803E0880_H

// Class and member names invented: no string or class-name evidence names this class. The name is
// taken from its vtable lbl_803E0880 (size 0x10, destructor 0x800176A4).

#include <revolution/types.h>
#include <stddef.h>
#include <climax/Heap.h>

/* A node of unk_803E0880: an unset value and its two neighbours. Name invented. */
struct unk_803E0880Node {
                    unk_803E0880Node() :
                        prev( NULL ),
                        next( NULL ) {}

    u32                     field_0x0;  // 0x0, never written by the list code
    unk_803E0880Node*       prev;       // 0x4
    unk_803E0880Node*       next;       // 0x8
};

// The class's vtable (size 0x10). The object's vptr is its address, and the constructor stores the
// same address again at 0x10 (purpose unknown).
extern "C" u8 __vt__12unk_803E0880[];

/*
 * A list of nodes that grows at the back, created with capacity nodes. The constructor is inline in
 * its only user, fn_800175A4. Its destructor is at 0x800176A4 and deletes the nodes.
 */
class unk_803E0880 {
public:
                    unk_803E0880( u32 capacity ) :
                        count( 0 ),
                        tail( NULL ),
                        head( NULL ),
                        field_0x10( __vt__12unk_803E0880 ),
                        field_0x14( 0 ),
                        field_0x18( 0 ),
                        field_0x1C( 0 ),
                        capacity( capacity ),
                        field_0x24( capacity ) {
                        for ( u32 i = 0; i < this->capacity; i++ ) {
                            Append( new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_803E0880Node() );
                        }
                    }
    virtual         ~unk_803E0880();

    /* Puts node after the last one. */
    void            Append( unk_803E0880Node* node ) {
                        node->prev = tail;
                        node->next = NULL;
                        if ( tail != NULL ) {
                            tail->next = node;
                        }
                        if ( head == NULL ) {
                            head = node;
                        }
                        tail = node;
                        count++;
                    }

    int             count;          // 0x4
    unk_803E0880Node* tail;         // 0x8
    unk_803E0880Node* head;         // 0xC
    void*           field_0x10;     // 0x10
    u32             field_0x14;     // 0x14
    u32             field_0x18;     // 0x18
    u32             field_0x1C;     // 0x1C
    u32             capacity;       // 0x20, the number of nodes the constructor makes
    u32             field_0x24;     // 0x24
};

#endif
