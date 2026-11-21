// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8002B7D4.h>

/* Takes a counted reference to p, if there is one. */
static inline void unk_AddRef( unk_PairBase* p ) {
    if ( p != NULL ) {
        p->refCount++;
    }
}

/* Drops a counted reference to p, deleting p with its last one. */
static inline void unk_Release( unk_PairBase* p ) {
    if ( p != NULL && --p->refCount == 0 ) {
        delete p;
    }
}

/*
====================
fn_8002BB18
====================
*/
// Not exact: retail keeps the incoming pointer in r31 from entry, the second count reads it from there.
// 0x8002BB18
extern "C" unk_Ref* fn_8002BB18( unk_Ref* self, unk_PairBase* ptr ) {
    unk_PairBase* incoming = ptr;
    unk_AddRef( ptr );
    unk_AddRef( incoming );
    unk_PairBase* old = self->ptr;
    self->ptr = incoming;
    unk_Release( old );
    unk_Release( incoming );
    return self;
}

/*
====================
fn_8002BBE8
====================
*/
// Not exact: retail re-tests this before the base destructor call (cause not found).
// 0x8002BBE8
extern "C" unk_PairNode* fn_8002BBE8( unk_PairNode* self, int deleteFlag ) {
    if ( self != NULL ) {
        fn_801336A0( self, 0 );
        if ( deleteFlag > 0 ) {
            operator delete( self );
        }
    }
    return self;
}

/*
====================
fn_8002BC44
====================
*/
// Not exact: the counter and the byte offset swap registers (r6/r7) against retail.
// 0x8002BC44
extern "C" void fn_8002BC44( unk_EntryTable* table ) {
    for ( int i = 0; i < table->header->count; i++ ) {
        unk_Entry* entry = table->items[i];
        entry->pairCount = 0;
    }
}

/*
====================
fn_8002B7D4
====================
*/
// Pairs every two entries of the range (i before j) and pushes each pair object onto the pair
// lists of both entries. Not exact (63%): the reference counting is written as plain helpers, not
// the retail's inline sequence, and the pair's vptr store is not reproduced.
// 0x8002B7D4
extern "C" void fn_8002B7D4( unk_EntryRange* range ) {
    for ( unk_Entry** i = range->begin; i != range->end; i++ ) {
        for ( unk_Entry** j = i; j != range->end; j++ ) {
            if ( j == i ) {
                continue;
            }
            unk_Entry* a = *i;
            unk_Entry* b = *j;

            unk_Pair* forward = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_Pair( a, b );
            unk_AddRef( forward );
            unk_AddRef( forward );
            unk_Release( forward );
            unk_PairNode* nodeA = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_PairNode;
            nodeA->ptr = forward;
            unk_AddRef( forward );
            nodeA->next = a->pairHead;
            nodeA->prev = NULL;
            if ( a->pairHead != NULL ) {
                a->pairHead->prev = nodeA;
            }
            if ( a->pairTail == NULL ) {
                a->pairTail = nodeA;
            }
            a->pairHead = nodeA;
            a->pairCount++;
            unk_Release( forward );

            unk_Pair* backward = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_Pair( b, a );
            unk_AddRef( backward );
            unk_AddRef( backward );
            unk_Release( backward );
            unk_PairNode* nodeB = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_PairNode;
            nodeB->ptr = backward;
            unk_AddRef( backward );
            nodeB->next = b->pairHead;
            nodeB->prev = NULL;
            if ( b->pairHead != NULL ) {
                b->pairHead->prev = nodeB;
            }
            if ( b->pairTail == NULL ) {
                b->pairTail = nodeB;
            }
            b->pairHead = nodeB;
            b->pairCount++;
            unk_Release( backward );
        }
    }
}
