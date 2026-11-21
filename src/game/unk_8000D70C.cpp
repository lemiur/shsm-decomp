// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8000D090.h>
#include <game/unk_PhysicsManager.h>

/*
====================
unk_8000D090::~unk_8000D090

Takes the shape out of the space and frees it, unregisters the space from the physics manager
and frees the space and the bit table. The compiler adds the delete tail.
====================
*/
// 0x8000D70C
unk_8000D090::~unk_8000D090() {
    fn_8031E2FC( space, shape );
    fn_8030F724( shape );
    shape = NULL;
    if ( space != NULL ) {
        unk_PhysicsManager* manager = lbl_80529170;
        unk_PhysicsSpaceLink* link = manager->FindSpace( space );
        if ( link != NULL ) {
            if ( link->prev == NULL ) {
                manager->spaces = link->next;
            } else {
                link->prev->next = link->next;
            }
            if ( link->next == NULL ) {
                manager->lastSpace = link->prev;
            } else {
                link->next->prev = link->prev;
            }
            link->prev = NULL;
            link->next = NULL;
            manager->spaceCount--;
            delete link;
        }
        fn_8031E1F4( space );
        space = NULL;
    }
    if ( bits != NULL ) {
        fn_803256B4( bits );
        bits = NULL;
    }
}
