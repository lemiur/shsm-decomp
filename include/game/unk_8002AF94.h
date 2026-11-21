#ifndef GAME_UNK_8002AF94_H
#define GAME_UNK_8002AF94_H

// unit name unconfirmed: header and type names invented.

#include <revolution/types.h>
#include <climax/Heap.h>
#include <game/unk_80120188.h>
#include <game/unk_HUDRequest.h>

struct unk_EndList;

/* A node of unk_EndList; it holds one reference to its timed object. Names invented. */
struct unk_EndNode {
                    unk_EndNode( const unk_RefPtr< unk_80120188 >& obj ) :
                        item( obj ),
                        next( NULL ),
                        prev( NULL ) {}

    unk_RefPtr< unk_80120188 > item;    // 0x0
    unk_EndNode*   next;               // 0x4
    unk_EndNode*   prev;               // 0x8
};

/* A doubly linked list of unk_EndNode, with its count, head and tail. Names invented. */
struct unk_EndList {
    u8              field_0x0[0x14];
    int             count;              // 0x14
    unk_EndNode*   head;               // 0x18
    unk_EndNode*   tail;               // 0x1C

    /* Links node in at the head of the list. */
    void            PushFront( unk_EndNode* node ) {
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
};

extern "C" {
void fn_8002AF94( void* owner, void* target, float dist, unk_EndList* listA, unk_EndList* listB );
}

#endif
