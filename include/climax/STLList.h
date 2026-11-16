#ifndef CLIMAX_STLLIST_H
#define CLIMAX_STLLIST_H

// Names and layout: Gormiti DWARF (ListUtils::STLList, game/System/Core/STLList.h) and the retail
// instantiations (vtable at 0, then size, head, tail; a node holds the element and both
// neighbours). Only the members the audio layer's headers use are declared; the bodies are the
// template's instantiations in climax/Audio/System.

#include <revolution/types.h>
#include <stddef.h>
#include <climax/Heap.h>

namespace ListUtils {

template< class T >
class STLList {
public:
    struct Node {
                        Node( const T& e ) : mElem( e ), mNext( NULL ), mPrev( NULL ) {}

        T               mElem;      // 0x00
        Node*           mNext;      // 0x04
        Node*           mPrev;      // 0x08
    };

    // Walks the list from the head (Gormiti: STLList.h:259; retail inlines it).
    class Iterator {
    public:
                        Iterator( Node* node ) : mNode( node ) {}

        T&              operator*() const { return mNode->mElem; }
        Iterator&       operator++() { mNode = mNode->mNext; return *this; }
        Iterator&       operator--() { mNode = mNode->mPrev; return *this; }
        bool            End() const { return mNode == NULL; }

    private:
        Node*           mNode;
    };

                        // Gormiti: in the class, STLList.h:43 (one line; retail inlines it).
                        STLList() : mSize( 0 ), mHead( NULL ), mTail( NULL ) {}
    virtual             ~STLList();

    // Gormiti: STLList.h:61, returning the node, from which the caller's Iterator is built; an
    // accessor returning an Iterator schedules a local list's vtable store after its first member
    // store (System::UpdateAsync and its neighbours).
    Node*               GetHead() const { return mHead; }
    int                 GetSize() const { return mSize; }
    Node*               GetTail() const { return mTail; }
    void                AddBack( const T& e );
    void                AddBack( Node* node );
    void                Remove( const T& e );
    template< class U >
    Node*               Find( const U& e ) const;
    void                Clear();
    void                Remove( Node* node );
    void                RemoveNoDelete( Node* node );

private:
    int                 mSize;      // 0x04
    Node*               mHead;      // 0x08
    Node*               mTail;      // 0x0C
};

// Gormiti: STLList.h:56.
template< class T >
STLList< T >::~STLList() {
    Clear();
}

// Gormiti: STLList.h:239.
template< class T >
void STLList< T >::Clear() {
    while ( mHead != NULL ) {
        Remove( mHead );
    }
}

// Gormiti: STLList.h:217.
template< class T >
void STLList< T >::Remove( Node* node ) {
    if ( node != NULL ) {
        RemoveNoDelete( node );
        delete node;
    }
}

// Gormiti: STLList.h:117 (AddBack).
template< class T >
void STLList< T >::AddBack( const T& e ) {
    Node* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) Node( e );
    AddBack( node );
}

template< class T >
void STLList< T >::AddBack( Node* node ) {
    node->mPrev = mTail;
    node->mNext = NULL;
    if ( mTail != NULL ) {
        mTail->mNext = node;
    }
    if ( mHead == NULL ) {
        mHead = node;
    }
    mTail = node;
    mSize++;
}

template< class T >
void STLList< T >::Remove( const T& e ) {
    Remove( Find( e ) );
}

template< class T >
template< class U >
typename STLList< T >::Node* STLList< T >::Find( const U& e ) const {
    for ( Node* node = mHead; node != NULL; node = node->mNext ) {
        if ( node->mElem == e ) {
            return node;
        }
    }
    return NULL;
}

template< class T >
void STLList< T >::RemoveNoDelete( Node* node ) {
    if ( node->mPrev == NULL ) {
        mHead = node->mNext;
    } else {
        node->mPrev->mNext = node->mNext;
    }
    if ( node->mNext == NULL ) {
        mTail = node->mPrev;
    } else {
        node->mNext->mPrev = node->mPrev;
    }
    node->mPrev = NULL;
    node->mNext = NULL;
    mSize--;
}

}

#endif
