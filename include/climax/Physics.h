#ifndef CLIMAX_PHYSICS_H
#define CLIMAX_PHYSICS_H

// unit name unconfirmed: header name invented. RenderWare Physics (Rwp, 0x802F5000-0x8032BA54) has
// no public SDK headers; the layouts and names below are placeholders taken from use.

#include <climax/AttributeHandler.h>
#include <climax/Frame.h>

/* A collision shape of a body. */
struct unk_RwpShape;

/* The simulation a body is added to. */
struct unk_RwpWorld;

/* What a body is placed in: its first word is its world (fn_8031E364 reads it). */
struct unk_RwpSpace;

/* A 0x20-byte entry of a simulation's state array; the vector at 0x10 is cleared to stop a body. */
struct unk_RwpState {
    u8          field_0x0[0x10];
    unk_RwV3d   field_0x10;     // 0x10
    u32         field_0x1C;     // 0x1C
};

struct unk_RwpStates {
    u8              field_0x0[0x8];
    unk_RwpState*   states;     // 0x8
};

struct unk_RwpSimulation {
    u8              field_0x0[0x10];
    unk_RwpStates*  field_0x10;     // 0x10
};

/* Where a body's state lives: a simulation and an index into its state array. */
struct unk_RwpStateRef {
    unk_RwpSimulation*  simulation;     // 0x0
    int                 index;          // 0x4
};

struct unk_RwpBodyParts {
    unk_RwpShape*       shape;      // 0x0
    unk_RwpStateRef*    state;      // 0x4, NULL while the body has no state
    unk_RwMatrix*       matrix;     // 0x8
};

/* A link of a body's contact list (0x2C into the body). */
struct unk_RwpContact {
    void*               field_0x0;  // 0x0
    u8                  field_0x4[0xC];
    unk_RwpContact*     next;       // 0x10
};

/* A rigid body (rwpOBJTYPEBODY, presumed). */
struct unk_RwpBody {
    u8                  field_0x0[0x10];
    unk_RwpBodyParts*   parts;      // 0x10
    u8                  field_0x14[0xC];
    u16                 index;      // 0x20, the body's number in its world (read by the Rwp functions)
    u16                 field_0x22;
    unk_RwpWorld*       world;      // 0x24, NULL while the body is in no world
    u32                 field_0x28;
    unk_RwpContact*     contacts;   // 0x2C
};

// defined in 0x801AB878, unit not yet reconstructed: creates the body that the attribute packet's
// "rwpID_BODYDEF" chunk describes; NULL without one
unk_RwpBody* fn_801AB878( const unk_AttributePacket& packet );

// defined in RW Physics (C linkage, as RenderWare is C), unit not yet reconstructed
extern "C" {
void    fn_802FA9D4( unk_RwpBody* body );                                   // destroys the body
void    fn_802FAB10( unk_RwpBody* body );
void    fn_80310778( unk_RwpShape* shape, float value );
void    fn_80310880( unk_RwpShape* shape, float value );
void    RwpMgrSetMoving( unk_RwpWorld* world, unk_RwpBody* body, int set );     // sets or clears the body's bit
void    RwpMgrActivate( unk_RwpWorld* world, unk_RwpBody* body );
void    RwpMgrDeactivate( unk_RwpWorld* world, unk_RwpBody* body );
void    RwpMgrSetGroup( unk_RwpWorld* world, unk_RwpBody* body, u16 group );
void    fn_8032110C( unk_RwpBody* body, int arg );
}

#endif
