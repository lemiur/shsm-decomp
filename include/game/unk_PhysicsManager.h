#ifndef GAME_UNK_PHYSICSMANAGER_H
#define GAME_UNK_PHYSICSMANAGER_H

// unit name unconfirmed: header name invented.

#include <string.h>
#include <revolution/types.h>
#include <climax/Heap.h>
#include <climax/Physics.h>

/* A space registered with the physics manager, linked both ways. Name invented. */
struct unk_PhysicsSpaceLink {
                            unk_PhysicsSpaceLink( unk_RwpSpace* s ) : space( s ), next( NULL ), prev( NULL ) {}

    unk_RwpSpace*           space;  // 0x0
    unk_PhysicsSpaceLink*   next;   // 0x4
    unk_PhysicsSpaceLink*   prev;   // 0x8
};

// The vtables of the two list classes the manager embeds (defined in unit game/unk_8000EC94.cpp). The
// manager's constructor stores their addresses as the lists' vptrs; the lists are not modelled as members yet.
extern void* __vt__12unk_803DFC38[3];
extern void* __vt__12unk_NodeList[3];

/*
 * The second word of the placement passed to Insert (fn_8000F0F4). Name invented; only the
 * word that selects the list end is known.
 */
struct unk_PhysicsPlacement {
    u32                     field_0x0;  // 0x0
    int                     atHead;     // 0x4, 1 links the body at the head of the active list; otherwise the tail
};

/*
 * A link of the free or the active list. The body and the placement argument are stored in
 * it; the body keeps the link in managerNode. Name invented.
 */
struct unk_PhysicsNode {
    unk_RwpBody*            body;   // 0x0
    void*                   arg;    // 0x4
    unk_PhysicsNode*        next;   // 0x8
    unk_PhysicsNode*        prev;   // 0xC

                            unk_PhysicsNode() {
                                next = 0;
                                prev = 0;
                            }
};

/*
 * Places bodies in the registered spaces (the instance is lbl_80529170). Role name invented; the
 * list members follow their use in the functions of unit game/unk_PhysicsManager.cpp.
 */
class unk_PhysicsManager {
public:
    unk_PhysicsManager() {
        field_0x14 = __vt__12unk_NodeList;
        freeCount = 0;
        freeHead = 0;
        freeTail = 0;
        field_0x24 = __vt__12unk_NodeList;
        activeCount = 0;
        activeHead = 0;
        activeTail = 0;
        field_0x7c0 = __vt__12unk_803DFC38;
        spaceCount = 0;
        spaces = 0;
        lastSpace = 0;
        field_0x0 = 0.0f;
        field_0x4 = -10.0f;
        field_0x8 = 0.0f;
        field_0xc = 0.2f;
        field_0x10 = 0.2f;
        nodeLimit = 0x20;
        budget = 0x20;

        for (u32 i = 0; i < nodeLimit; i++) {
            unk_PhysicsNode* node = new (GAME_HEAP, GAME_HEAP_FLAGS) unk_PhysicsNode;
            node->next = freeHead;
            node->prev = 0;
            if (freeHead != 0) {
                freeHead->prev = node;
            }
            if (freeTail == 0) {
                freeTail = node;
            }
            freeHead = node;
            freeCount++;
        }

        memset(field_0x3C, 0, 0x3C0);
        memset(field_0x3FC, 0, 0x3C0);
        enabled = true;
    }

    /* The link of space; NULL when it is not registered. Name invented. */
    unk_PhysicsSpaceLink*   FindSpace( unk_RwpSpace* space ) {
                                for ( unk_PhysicsSpaceLink* link = spaces; link != NULL; link = link->next ) {
                                    if ( link->space == space ) {
                                        return link;
                                    }
                                }
                                return NULL;
                            }

    // defined in unit game/unk_PhysicsManager.cpp (0x8000F0F4, 0x8000F278) and in units not yet
    // reconstructed (0x8000F5E4, 0x8000F6AC): fn_8000F5E4 adds body to space's world (RwpMgrInsertObj)
    void                    fn_8000F0F4( unk_RwpBody* body, void* arg );
    void                    fn_8000F278( unk_RwpBody* body );
    void                    fn_8000F5E4( unk_RwpSpace* space, unk_RwpBody* body, int arg, u32 flags );
    // defined in game/unk_PhysicsManager.cpp: the same body as fn_8000F5E4; the instance (this) is not used
    void                    fn_8000F648( unk_RwpSpace* space, unk_RwpBody* body, int arg, u32 flags );
    void                    fn_8000F6AC( unk_RwpBody* body );
    // defined in 0x8000F71C, unit game/unk_8000F71C.cpp: removes the body from its world; true when it was in one
    bool                    fn_8000F71C( unk_RwpBody* body );

    float                   field_0x0;
    float                   field_0x4;
    float                   field_0x8;
    float                   field_0xc;
    float                   field_0x10;
    void*                   field_0x14;         // 0x14, vptr: __vt__12unk_NodeList
    int                     freeCount;          // 0x18
    unk_PhysicsNode*        freeHead;           // 0x1C
    unk_PhysicsNode*        freeTail;           // 0x20
    void*                   field_0x24;         // 0x24, vptr: __vt__12unk_NodeList
    int                     activeCount;        // 0x28
    unk_PhysicsNode*        activeHead;         // 0x2C
    unk_PhysicsNode*        activeTail;         // 0x30
    u32                     nodeLimit;          // 0x34, the number of nodes the constructor creates
    int                     budget;             // 0x38, moves with the active list
    u8                      field_0x3C[0x3C0];  // 0x3C: two arrays of sixteen 0x3C-byte records (not reconstructed)
    u8                      field_0x3FC[0x3C0];
    bool                    enabled;            // 0x7BC
    void*                   field_0x7c0;        // 0x7C0, vptr: __vt__12unk_803DFC38
    u32                     spaceCount;         // 0x7C4, bumped for each space created; name invented
    unk_PhysicsSpaceLink*   spaces;             // 0x7C8
    unk_PhysicsSpaceLink*   lastSpace;          // 0x7CC
};

extern unk_PhysicsManager* lbl_80529170;

/* Counter that each body insertion bumps and fn_8000F71C drops on removal. Name invented. */
extern int lbl_80529178;

#endif
