// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <climax/Heap.h>
#include <game/unk_ClumpBounds.h>

// The destructor's body is inline here, as in the retail unit: fn_80025674 deletes its bounds
// through it. The out-of-line copy is in another unit. CheckAtomic makes the
// spheres with new[], and the node frees them with a single-object delete, as retail does.
inline unk_ClumpBounds::~unk_ClumpBounds() {
    while ( nodes != NULL ) {
        unk_ClumpBoundsNode* node = nodes;
        nodes = node->next;
        delete node->spheres;
        delete node;
    }
}

/*
====================
fn_80025674

Not matched: 2 of 55 instructions differ, the loop test's load of nodes (retail reloads it into r0,
the body into r30; MWCC reuses the test's load for node). Register/schedule wall; the float
constant is the compiler's @174 where retail names lbl_8052B840.
====================
*/
// 0x80025674
unk_ClumpBounds* fn_80025674( unk_RpClump* clump ) {
    if ( clump == NULL ) {
        return NULL;
    }
    unk_ClumpBounds* bounds = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ClumpBounds;
    bounds->clump = clump;
    RpClumpForAllAtomics( clump, fn_8002563C, bounds );
    if ( bounds->nodes == NULL ) {
        delete bounds;
        return NULL;
    }
    return bounds;
}
