// unit name unconfirmed: the class is named after its constructor fn_8004BC94 (it stores the vtable lbl_803E0C60); no string names it.

#include <game/unk_8004BC94.h>

/*
 * Removes a node from the active list and clears its links.
 */
static void unlinkNode( unk_InterpList* list, unk_InterpNode* node )
{
    if ( node->prev != NULL ) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }
    if ( node->next != NULL ) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }
    node->prev = NULL;
    node->next = NULL;
    list->count--;
}

/*
 * Appends a node to the cache of released nodes.
 */
static void cacheNode( unk_InterpList* list, unk_InterpNode* node )
{
    node->prev = list->cacheTail;
    node->next = NULL;
    if ( list->cacheTail != NULL ) {
        list->cacheTail->next = node;
    }
    if ( list->cacheHead == NULL ) {
        list->cacheHead = node;
    }
    list->cacheTail = node;
    list->cacheCount++;
}

/*
====================
unk_8004BC94::~unk_8004BC94
====================
*/
// Not matched: the base destructor's guard and vtable store (lbl_803E0D80) are inlined here, and the
// register numbering differs (base vtable not declared; see the header).
// 0x80020564
unk_8004BC94::~unk_8004BC94()
{
    unk_InterpList* list;
    unk_InterpNode* node;

    *(void**)this = lbl_803E0D80;
    if ( interpolator != NULL ) {
        list = system->interpList;
        node = list->head;
        while ( node != NULL ) {
            if ( node->interpolator == interpolator ) {
                break;
            }
            node = node->next;
        }
        if ( node != NULL ) {
            if ( list->count + list->cacheCount <= 20 ) {
                unlinkNode( list, node );
                cacheNode( list, node );
            } else {
                unlinkNode( list, node );
                delete node;
                RtAnimInterpolatorDestroy( interpolator );
            }
        }
        interpolator = NULL;
    }
}
