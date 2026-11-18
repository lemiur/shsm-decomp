#ifndef GAME_UNK_PHYSICSMANAGER_H
#define GAME_UNK_PHYSICSMANAGER_H

// unit name unconfirmed: header name invented.

#include <climax/Physics.h>

/* A space registered with the physics manager. Name invented. */
struct unk_PhysicsSpaceLink {
    unk_RwpSpace*           space;  // 0x0
    unk_PhysicsSpaceLink*   next;   // 0x4
};

/*
 * Places bodies in the registered spaces (unit 0x8000D090-, not yet reconstructed; the instance
 * is lbl_80529170). Role name invented; only the members used so far are known.
 */
class unk_PhysicsManager {
public:
    /* The link of space; NULL when it is not registered. Name invented. */
    unk_PhysicsSpaceLink*   FindSpace( unk_RwpSpace* space ) {
                                for ( unk_PhysicsSpaceLink* link = spaces; link != NULL; link = link->next ) {
                                    if ( link->space == space ) {
                                        return link;
                                    }
                                }
                                return NULL;
                            }

    // defined in 0x8000F0F4, 0x8000F278, 0x8000F5E4 and 0x8000F6AC, unit not yet reconstructed:
    // fn_8000F5E4 adds body to space's world (RwpMgrInsertObj)
    void                    fn_8000F0F4( unk_RwpBody* body, void* arg );
    void                    fn_8000F278( unk_RwpBody* body );
    void                    fn_8000F5E4( unk_RwpSpace* space, unk_RwpBody* body, int arg, u32 flags );
    void                    fn_8000F6AC( unk_RwpBody* body );

    u8                      field_0x0[0x7C8];
    unk_PhysicsSpaceLink*   spaces;     // 0x7C8
};

extern unk_PhysicsManager* lbl_80529170;

#endif
