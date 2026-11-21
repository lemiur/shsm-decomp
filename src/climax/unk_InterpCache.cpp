// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <climax/unk_InterpCache.h>

/* The node of the active list whose interpolator is key, or NULL. */
static unk_ListNode< RtAnimInterpolator* >* FindInterpNode( unk_InterpList& list, RtAnimInterpolator* key ) {
    unk_ListNode< RtAnimInterpolator* >* node = list.head;
    while ( node != NULL ) {
        if ( node->data == key ) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/*
====================
unk_InterpCache::~unk_InterpCache
====================
*/
// Wall (control flow): retail repeats the this-null test at 0x800203C4 (dead), not reproduced;
// referent: the vptr is lbl_803E0D80, a vtable shared with other units, not renamed here.
// 0x8002039C
unk_InterpCache::~unk_InterpCache() {
    if ( interpolator != NULL ) {
        RtAnimInterpolator* interp = interpolator;
        unk_ListNode< RtAnimInterpolator* >* node = FindInterpNode( owner->pool->active, interp );
        unk_InterpPool* pool = owner->pool;
        if ( node != NULL ) {
            if ( pool->active.count + pool->spare.count <= 20 ) {
                unk_InterpList& active = pool->active;
                unk_InterpList& spare = pool->spare;
                if ( node->prev == NULL ) {
                    active.head = node->next;
                } else {
                    node->prev->next = node->next;
                }
                if ( node->next == NULL ) {
                    active.tail = node->prev;
                } else {
                    node->next->prev = node->prev;
                }
                node->prev = NULL;
                node->next = NULL;
                active.count--;

                node->prev = spare.tail;
                node->next = NULL;
                if ( spare.tail != NULL ) {
                    spare.tail->next = node;
                }
                if ( spare.head == NULL ) {
                    spare.head = node;
                }
                spare.tail = node;
                spare.count++;
            } else {
                pool->active.Remove( node );
                RtAnimInterpolatorDestroy( interp );
            }
        }
        interpolator = NULL;
    }
}
