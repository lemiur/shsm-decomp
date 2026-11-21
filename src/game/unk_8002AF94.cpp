// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// Parked (NonMatching): the body of fn_8002AF94 is exact, but the flip is blocked by the object's
// layout the gate reports: its .sdata2 section is 8-aligned (retail's 1.0f sits at 0x8052B8B4), and
// an out-of-line copy of unk_RefPtr<unk_80120188>::~unk_RefPtr (132 bytes) is emitted in the unit.

#include <game/unk_8002AF94.h>
#include <climax/Heap.h>

/*
====================
fn_8002AF94

Builds a pair of timed objects, each built for the other's list, and links each one into a
list: the first into listA, the second into listB. The two ends refer to each other.
====================
*/
// 0x8002AF94
extern "C" void fn_8002AF94( void* owner, void* target, float dist, unk_EndList* listA, unk_EndList* listB ) {
    float lifetime = 1.0f;
    unk_RefPtr< unk_80120188 > a;
    unk_RefPtr< unk_80120188 > b;

    unk_80120188* timedA = (unk_80120188*)operator new( 0x28, GAME_HEAP, GAME_HEAP_FLAGS );
    if ( timedA != NULL ) {
        timedA = fn_80120188( timedA, 1, listB, dist, lifetime );
    }
    a = timedA;
    /* The temporary reference lives until the node has taken its own, so it is released after the push. */
    {
        unk_RefPtr< unk_80120188 > ref( a );
        listA->PushFront( new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_EndNode( ref ) );
    }

    unk_80120188* timedB = (unk_80120188*)operator new( 0x28, GAME_HEAP, GAME_HEAP_FLAGS );
    if ( timedB != NULL ) {
        timedB = fn_80120188( timedB, 1, listA, dist, lifetime );
    }
    b = timedB;
    {
        unk_RefPtr< unk_80120188 > ref( b );
        listB->PushFront( new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_EndNode( ref ) );
    }

    a->partner = b;
    b->partner = a;
}
