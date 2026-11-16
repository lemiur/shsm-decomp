#ifndef CLIMAX_UNK_LIST_H
#define CLIMAX_UNK_LIST_H

// Header and type names invented (the Climax container's own names are unknown); the layout is
// the one the two vtables 0x80429D2C and 0x80429D44 and their destructors share.

#include <revolution/types.h>
#include <stddef.h>
#include <climax/Memory.h>

/* A node of unk_List: the value and its two neighbours. */
template< class T >
struct unk_ListNode {
                    unk_ListNode( T value ) :
                        data( value ),
                        next( NULL ),
                        prev( NULL ) {}

    T                   data;
    unk_ListNode*       next;
    unk_ListNode*       prev;
};

/*
 * A doubly linked list of values with a virtual destructor, which empties it. New values go to the
 * front.
 */
template< class T >
class unk_List {
public:
    typedef unk_ListNode< T > Node;

                    unk_List() :
                        count( 0 ),
                        head( NULL ),
                        tail( NULL ) {}
    virtual         ~unk_List() {
                        while ( head != NULL ) {
                            Remove( head );
                        }
                    }

    /* Puts value in front of the others. */
    void            Push( T value ) {
                        Node* node = new( GAME_HEAP, GAME_HEAP_FLAGS ) Node( value );
                        node->next = head;
                        node->prev = NULL;
                        if ( head != NULL ) {
                            head->prev = node;
                        }
                        if ( tail == NULL ) {
                            tail = node;
                        }
                        head = node;
                        count++;
                    }

    /* Unlinks node and deletes it; does nothing for NULL. */
    void            Remove( Node* node ) {
                        if ( node != NULL ) {
                            if ( node->prev == NULL ) {
                                head = node->next;
                            } else {
                                node->prev->next = node->next;
                            }
                            if ( node->next == NULL ) {
                                tail = node->prev;
                            } else {
                                node->next->prev = node->prev;
                            }
                            node->prev = NULL;
                            node->next = NULL;
                            count--;
                            delete node;
                        }
                    }

    int             count;      // 0x4
    Node*           head;       // 0x8
    Node*           tail;       // 0xC
};

#endif
