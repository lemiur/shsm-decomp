/*
 * Intrusive doubly linked list node used throughout FMOD's internals (the system list, the async
 * thread list). A list head is a node linked to itself. Class and member names invented (FMOD
 * convention); the layout is from retail use (fmod.cpp, fmod_async.cpp).
 */
#ifndef _FMOD_LINKEDLIST_H
#define _FMOD_LINKEDLIST_H

#include <stddef.h>

namespace FMOD {

class LinkedListNode {
public:
    LinkedListNode *mNodeNext;      // 0x0
    LinkedListNode *mNodePrev;      // 0x4
    void           *mNodeData;      // 0x8

    LinkedListNode() {
        mNodeNext = this;
        mNodePrev = this;
        mNodeData = NULL;
    }

    LinkedListNode *getNext() const { return mNodeNext; }

    bool isEmpty() const {
        if ( mNodeNext == this && mNodePrev == this ) {
            return true;
        }
        return false;
    }

    // Number of nodes between this head and the end of the list (fmod_eventgroupi.cpp).
    int count() const {
        int             n       = 0;
        LinkedListNode *current = mNodeNext;
        while ( current != this ) {
            n++;
            current = current->mNodeNext;
        }
        return n;
    }

    // The node index places after this head, NULL when the list is shorter (name invented).
    LinkedListNode *getNodeByIndex( int index ) const {
        LinkedListNode *current = mNodeNext;
        if ( current == this ) {
            return NULL;
        }
        while ( index-- > 0 ) {
            current = current->mNodeNext;
            if ( current == this ) {
                return NULL;
            }
        }
        return current;
    }

    // ---- added for fmod_eventgroupi ----
    // Whether node is in the list headed by this node; the head itself is compared first (name invented).
    bool contains( const LinkedListNode *node ) const {
        const LinkedListNode *current = mNodeNext;
        do {
            if ( current == node ) {
                return true;
            }
            current = current->mNodeNext;
        } while ( current != this );
        return false;
    }

    // Makes this node an empty list (linked to itself, no data) (name invented).
    void initNode() {
        mNodeNext = this;
        mNodePrev = this;
        mNodeData = NULL;
    }

    void setData( void *data ) { mNodeData = data; }
    // ---- end fmod_eventgroupi ----

    // ---- added for fmod_eventprojecti ----
    // The place node has in the list headed by this node, -1 when it is not in it (name invented).
    int getNodeIndex( const LinkedListNode *node ) const {
        LinkedListNode *current = mNodeNext;
        int             index   = 0;
        while ( current != this ) {
            if ( current == node ) {
                return index;
            }
            current = current->mNodeNext;
            index++;
        }
        return -1;
    }
    // ---- end fmod_eventprojecti ----
    // The node's data (fmod_dsp_connectionpool.cpp).
    void *getData() const { return mNodeData; }

    // Links this node in directly after node.
    void addAfter( LinkedListNode *node ) {
        mNodeNext = node->mNodeNext;
        mNodePrev = node;
        mNodeNext->mNodePrev = this;
        mNodePrev->mNodeNext = this;
    }

    // Links this node in directly before node (at the end of a list whose head is node).
    void addBefore( LinkedListNode *node ) {
        mNodePrev = node->mNodePrev;
        mNodeNext = node;
        mNodeNext->mNodePrev = this;
        mNodePrev->mNodeNext = this;
    }

    // Unlinks this node and leaves it linked to itself.
    void removeNode() {
        mNodePrev->mNodeNext = mNodeNext;
        mNodeNext->mNodePrev = mNodePrev;
        mNodePrev = this;
        mNodeNext = this;
        mNodeData = NULL;
    }
};

// A list node ordered by a priority key (Gormiti DWARF: SortedLinkedListNode, size 0x10).
class SortedLinkedListNode : public LinkedListNode {
public:
    unsigned int mNodePriority;     // 0xC

    // A node not yet given a priority (name invented; the constructor stores -1).
    enum { NODE_PRIORITY_NONE = 0xFFFFFFFF };

    SortedLinkedListNode() {
        mNodePriority = NODE_PRIORITY_NONE;
        initNode();
    }
};

} // namespace FMOD

#endif /* _FMOD_LINKEDLIST_H */
