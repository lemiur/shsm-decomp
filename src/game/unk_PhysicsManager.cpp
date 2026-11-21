// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_PhysicsManager.h>
#include <game/unk_803E8E64.h>

// Data owned by this unit (names are the symbols.txt names):
//   .sbss 0x80529174 size 0x4 lbl_80529174
int lbl_80529174;

// fn_8000F350 (0x8000F350): not reconstructed (the active-list walk over the contact records);
// declared only, so the retail body is linked and this unit stays NonMatching. Blockers: the two
// 16-entry arrays at +0x3C and +0x3FC hold 0x3C-byte records whose layout is not modelled; the
// contact's other object (+0xC, members at 0xB0, 0xB4 and 0xD0) has no type; and the calls go
// through the placement's vtable slots 3 and 4 while the placement's other slots are unknown.
extern "C" void fn_8000F350( unk_PhysicsManager* manager, float dt );

/*
====================
fn_8000EFA4
====================
*/
// Not matched (31%), wall: control flow. Retail clears the 0x1000-byte table with a 32-trip loop of
// 32 stores. A flat int[0x400] clear unrolls as 512 trips of 2; a nested 32x32 clear (or a row
// helper) unrolls the outer loop by two, 16 trips of 64 stores. The table's zero contents make the
// read a constant.
// 0x8000EFA4
extern "C" void fn_8000EFA4() {
    int table[0x400] = { 0 };
    lbl_80529174 = table[lbl_80529174];
}

/*
====================
fn_8000F05C
====================
*/
// The receiver is the first argument: the symbol carries no class name (dtk did not pair it).
// 0x8000F05C
extern "C" void fn_8000F05C( unk_PhysicsManager* manager, unk_RwpSpace* space, float dt ) {
    if ( !manager->enabled ) {
        return;
    }
    unk_RwpWorld* world = fn_8031E35C( space );
    fn_80322074( world, dt );
    RwpPipelineGenerateContacts( world );
    fn_80322E04( world );
    fn_80322A78( world );
    fn_80322DC4( world );
    fn_8000EFA4();
    fn_80325530( world, dt );
    fn_8000F350( manager, dt );
}

/*
====================
unk_PhysicsManager::fn_8000F0F4
====================
*/
// 0x8000F0F4
void unk_PhysicsManager::fn_8000F0F4( unk_RwpBody* body, void* arg ) {
    unk_PhysicsPlacement* placement = static_cast<unk_PhysicsPlacement*>( arg );
    unk_PhysicsNode* node;
    // retail stores through the node even when the free list is empty (the original's
    // unguarded store; kept)
    if ( placement->atHead == 1 ) {
        node = freeHead;
        if ( node != NULL ) {
            if ( node->prev == NULL ) {
                freeHead = node->next;
            } else {
                node->prev->next = node->next;
            }
            if ( node->next == NULL ) {
                freeTail = node->prev;
            } else {
                node->next->prev = node->prev;
            }
            node->prev = NULL;
            node->next = NULL;
            freeCount--;

            node->next = activeHead;
            node->prev = NULL;
            if ( activeHead != NULL ) {
                activeHead->prev = node;
            }
            if ( activeTail == NULL ) {
                activeTail = node;
            }
            activeHead = node;
            activeCount++;
            budget--;
        }
    } else {
        node = freeHead;
        if ( node != NULL ) {
            if ( node->prev == NULL ) {
                freeHead = node->next;
            } else {
                node->prev->next = node->next;
            }
            if ( node->next == NULL ) {
                freeTail = node->prev;
            } else {
                node->next->prev = node->prev;
            }
            node->prev = NULL;
            node->next = NULL;
            freeCount--;

            node->prev = activeTail;
            node->next = NULL;
            if ( activeTail != NULL ) {
                activeTail->next = node;
            }
            if ( activeHead == NULL ) {
                activeHead = node;
            }
            activeTail = node;
            activeCount++;
            budget--;
        }
    }

    node->body = body;
    node->arg = arg;
    body->managerNode = node;
    body->flags |= 0x40;
}

/*
====================
unk_PhysicsManager::fn_8000F278
====================
*/
// 0x8000F278
void unk_PhysicsManager::fn_8000F278( unk_RwpBody* body ) {
    body->flags &= ~0x40;
    unk_PhysicsNode* node = activeHead;
    while ( node != NULL ) {
        if ( node->body == body ) {
            if ( node->prev == NULL ) {
                activeHead = node->next;
            } else {
                node->prev->next = node->next;
            }
            if ( node->next == NULL ) {
                activeTail = node->prev;
            } else {
                node->next->prev = node->prev;
            }
            node->prev = NULL;
            node->next = NULL;
            activeCount--;

            node->next = freeHead;
            node->prev = NULL;
            if ( freeHead != NULL ) {
                freeHead->prev = node;
            }
            if ( freeTail == NULL ) {
                freeTail = node;
            }
            freeHead = node;
            freeCount++;
            budget++;
            node = NULL;
        } else {
            node = node->next;
        }
    }
}

/*
====================
unk_803E8E64::fn_8000F5DC
====================
*/
// 0x8000F5DC
void unk_803E8E64::fn_8000F5DC() {
}

/*
====================
unk_803E8E64::fn_8000F5E0
====================
*/
// 0x8000F5E0
void unk_803E8E64::fn_8000F5E0() {
}

/*
====================
unk_PhysicsManager::fn_8000F5E4

Adds body to the world of space with arg and flags, and raises lbl_80529178.
====================
*/
// 0x8000F5E4
void unk_PhysicsManager::fn_8000F5E4( unk_RwpSpace* space, unk_RwpBody* body, int arg, u32 flags ) {
    unk_RwpWorld* world = fn_8031E364( space );
    lbl_80529178++;
    RwpMgrInsertObj( world, body, arg, flags );
}

/*
====================
unk_PhysicsManager::fn_8000F648

Adds the body to the space's world and counts the insertion. The body is identical to fn_8000F5E4;
both are kept as the original's two members.
====================
*/
// 0x8000F648
void unk_PhysicsManager::fn_8000F648( unk_RwpSpace* space, unk_RwpBody* body, int arg, u32 flags ) {
    unk_RwpWorld* world = fn_8031E364( space );
    lbl_80529178++;
    RwpMgrInsertObj( world, body, arg, flags );
}
