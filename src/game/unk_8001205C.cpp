// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8001205C.h>
#include <climax/Heap.h>

/*
====================
unk_StartNodeGroup::fn_8001205C
Wall (register/schedule): the path-A counter loads, where retail hoists the this copy among
them, and the start-index store's base register; the rest matches.
====================
*/
// 0x8001205C
void unk_StartNodeGroup::fn_8001205C( unk_StartNode* node ) {
    if ( entries == NULL ) {
        unallocatedA++;
        nodeCount++;
        total += fn_8000F804( node );
        total += fn_8000F8F8( node, this );
        return;
    }

    unk_NodeEntry* entry = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_NodeEntry;
    if ( entry != NULL ) {
        entry = fn_8000FC5C( entry, node, countA, this );
    }
    entries[countA++] = entry;
    records[node->set->index].items[node->index] = entry;

    unk_NodeProps* props = fn_80188218( node, "STE Start Node" );
    if ( props != NULL ) {
        unk_NodePropSlot* slot = fn_80189D10( props, "isStartNode" );
        if ( slot->value->isStartNode ) {
            int setIndex = node->set->index;
            startIndices[setIndex] = node->index;
        }
    }
}

/*
====================
unk_StartNodeGroup::fn_800121A0
Wall (register/schedule): the path-A counter loads, where retail hoists the this copy among
them; the rest matches.
====================
*/
// 0x800121A0
void unk_StartNodeGroup::fn_800121A0( unk_StartNode* node, int arg ) {
    if ( links == NULL ) {
        unallocatedB++;
        nodeCount++;
        total += fn_8000F804( node );
        total += fn_80011054( node, this, arg );
        return;
    }

    unk_NodeLink* link = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_NodeLink;
    if ( link != NULL ) {
        link = fn_800110FC( link, node, countB, this, arg );
    }
    links[countB++] = link;
    records[node->set->index].items[node->index] = link;
}

/*
====================
unk_StartNodeGroup::fn_800122A4
====================
*/
// 0x800122A4
void unk_StartNodeGroup::fn_800122A4() {
    if ( records == NULL ) {
        nodeCount++;
    }
}

/*
====================
fn_800122C0
====================
*/
// 0x800122C0
extern "C" unk_NodeChild* fn_800122C0( unk_NodeOwner* owner ) {
    unk_NodeChild* child = fn_801874FC( owner );
    if ( child == NULL ) {
        child = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_NodeChild;
        if ( child != NULL ) {
            child = fn_80011BB0( child, owner );
        }
    }
    if ( child == NULL ) {
        return NULL;
    }
    child->refCount++;
    return child;
}
