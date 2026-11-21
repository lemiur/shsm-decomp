#ifndef CLIMAX_PHYSICS_H
#define CLIMAX_PHYSICS_H

// unit name unconfirmed: header name invented. RenderWare Physics (Rwp, 0x802F5000-0x8032BA54) has
// no public SDK headers; the layouts and names below are placeholders taken from use.

#include <climax/AttributeHandler.h>
#include <climax/Frame.h>

/* The geometry a shape describes: its kind (0x0) and flags (0x40; bit 2 means the shape has no
 * local transform, so its matrix product takes the identity). Names invented. */
struct unk_RwpGeometry {
    u16         type;           // 0x0
    u8          field_0x2[0x3E];
    u32         flags;          // 0x40
};

struct unk_RwpBody;

/* A collision shape of a body: its local transform (a matrix at 0x0), two float extents and the
 * geometry it describes. Names invented; the offsets are the accesses of fn_8000E89C. */
struct unk_RwpShape {
    unk_RwMatrix        local;          // 0x00
    float               field_0x40;     // 0x40
    float               field_0x44;     // 0x44
    u8                  field_0x48[0x4];
    float               field_0x4C;     // 0x4C
    u8                  field_0x50[0xC];
    unk_RwpGeometry*    geometry;       // 0x5C
};

/* The simulation a body is added to. Its object table holds the bodies by their index; its bits at 0x60 mark the
 * bodies a flagged sweep skips (one bit per body index, read by fn_8000E89C). Layout partly known. */
struct unk_RwpWorld {
    u8                  field_0x0[0x44];
    u32                 objectCount;    // 0x44, the entries of objects a body index may name
    u8                  field_0x48[0x8];
    unk_RwpBody**       objects;        // 0x50
    u8                  field_0x54[0xC];
    u32*                bodyBits;       // 0x60
};

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

/* A bit table of a simulation: the word at 0x4 is the count its index formulas use; the bits start at 0xC.
   Names invented. */
struct unk_RwpBitTable {
    u32                 field_0x0;  // 0x0
    u32                 count;      // 0x4
    u32                 field_0x8;  // 0x8
    u32                 words[1];   // 0xC, bit i is words[i >> 5] bit (i & 31)
};

/* Sets and clears bit index. Names invented; the bodies are the inline bit operations of the table. */
inline void unk_RwpBitTableSet( unk_RwpBitTable* table, u32 index ) {
    table->words[ index >> 5 ] |= 1 << ( index & 31 );
}

inline void unk_RwpBitTableClear( unk_RwpBitTable* table, u32 index ) {
    table->words[ index >> 5 ] &= ~( 1 << ( index & 31 ) );
}

/* A link of a body's contact list (0x2C into the body). */
struct unk_RwpContact {
    void*               field_0x0;  // 0x0
    u8                  field_0x4[0xC];
    unk_RwpContact*     next;       // 0x10
};

/* A link of unk_PhysicsManager's free or active list. */
struct unk_PhysicsNode;

/* A rigid body (rwpOBJTYPEBODY, presumed). */
struct unk_RwpBody {
    u8                  field_0x0[0x8];
    u32                 flags;          // 0x8, bit 0x40 is set while the body is in unk_PhysicsManager's active list
    u16                 partCount;  // 0xC, the number of entries in parts
    u8                  field_0xE[0x2];
    unk_RwpBodyParts*   parts;      // 0x10
    u8                  field_0x14[0xC];
    u16                 index;      // 0x20, the body's number in its world (read by the Rwp functions)
    u16                 field_0x22;
    unk_RwpWorld*       world;      // 0x24, NULL while the body is in no world
    unk_PhysicsNode*    managerNode;    // 0x28, the body's link in unk_PhysicsManager's active list
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
void    RwpMgrRemoveObj( unk_RwpWorld* world, unk_RwpBody* body );
float   fn_802FDE64( unk_RwpShape* shape );     // reads the shape's 0x4C
float   fn_802FDE6C( unk_RwpShape* shape );     // reads the shape's 0x40
float   fn_8030195C( unk_RwpShape* shape );     // reads the shape's 0x44
float   fn_80301964( unk_RwpShape* shape );     // reads the shape's 0x40
void    RwpMgrSetMoving( unk_RwpWorld* world, unk_RwpBody* body, int set );     // sets or clears the body's bit
void    RwpMgrActivate( unk_RwpWorld* world, unk_RwpBody* body );
void    RwpMgrDeactivate( unk_RwpWorld* world, unk_RwpBody* body );
void    RwpMgrSetGroup( unk_RwpWorld* world, unk_RwpBody* body, u16 group );
void    RwpMgrInsertObj( unk_RwpWorld* world, unk_RwpBody* body, int arg, u32 flags );
void    fn_8032110C( unk_RwpBody* body, int arg );
unk_RwpBitTable* RwpBitTableCreate( u32 a, u32 b, u32 index );
void    fn_80325594( void* params, const void* def );
void    fn_80325634( void* params, u32 arg );
void    fn_8032B9C8( void* params, u32 a, u32 b );
void    fn_802FAC88( void* params, u32 a, u32 b );
void    fn_803255F8( void* params, u32 arg );
void    fn_80325624( void* params, u32 arg );
void    fn_8032562C( void* params, u32 arg );
unk_RwpSpace* fn_8031E078( void* params, const void* box, float radius );
unk_RwpShape* fn_8030E904( const void* def );
void    fn_8030F770( unk_RwpShape* shape, int arg );
void    fn_8030F768( unk_RwpShape* shape, float value );
void    fn_8031E288( unk_RwpSpace* space, unk_RwpShape* shape, int arg );
unk_RwpWorld* fn_8031E364( unk_RwpSpace* space );
void    fn_80320D48( unk_RwpWorld* world, unk_RwpBitTable* table );
void    fn_8032207C( void* target, const unk_RwV3d* position );
void    fn_8030F724( unk_RwpShape* shape );                                 // releases the shape's parts (placeholder)
void    fn_8031E1F4( unk_RwpSpace* space );                                 // destroys the space (placeholder)
void    fn_8031E2FC( unk_RwpSpace* space, unk_RwpShape* shape );            // takes the shape out of the space (placeholder)
void    fn_803256B4( unk_RwpBitTable* table );                              // releases the bit table (placeholder)
unk_RwpWorld* fn_8031E35C( unk_RwpSpace* space );                       // returns the space's world
void    fn_80322074( unk_RwpWorld* world, float dt );
void    RwpPipelineGenerateContacts( unk_RwpWorld* world );
void    fn_80322A78( unk_RwpWorld* world );
void    fn_80322DC4( unk_RwpWorld* world );
void    fn_80322E04( unk_RwpWorld* world );
void    fn_80325530( unk_RwpWorld* world, float dt );
}

#endif
