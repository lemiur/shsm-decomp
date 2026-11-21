#ifndef GAME_UNK_803E2BB0_H
#define GAME_UNK_803E2BB0_H

// Class and member names invented: no string names this class, so it takes its vtable address
// (__vt__12unk_803E2BB0 at 0x803E2BB0). It is a doubly linked list that owns its nodes, with the
// layout of climax/unk_List.h's template for a 4-byte element.

#include <revolution/types.h>

/* A node of unk_803E2BB0: the word the destructor never reads, and its two neighbours. */
struct unk_803E2BB0Node {
    u32                 field_0x0;  // 0x00
    unk_803E2BB0Node*   next;       // 0x04
    unk_803E2BB0Node*   prev;       // 0x08
};

class unk_803E2BB0 {
public:
                    unk_803E2BB0() :
                        count( 0 ),
                        head( NULL ),
                        tail( NULL ) {}
    /*
     * Empties the list: each Remove unlinks the head and deletes it. Defined in the class, as the
     * template in climax/unk_List.h is; the vtable's destructor slot is __dt__12unk_803E2BB0Fv.
     */
    virtual         ~unk_803E2BB0() {
                        while ( head != NULL ) {
                            Remove( head );
                        }
                    }

    /* Unlinks node and deletes it; does nothing for NULL. */
    void            Remove( unk_803E2BB0Node* node ) {
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

    int                 count;  // 0x04
    unk_803E2BB0Node*   head;   // 0x08
    unk_803E2BB0Node*   tail;   // 0x0C
};

#endif
