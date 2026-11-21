#ifndef GAME_CPHYSICSOBJECT_H
#define GAME_CPHYSICSOBJECT_H

#include <revolution/types.h>
#include <game/unk_8009D2E4.h>
#include <climax/AttributeHandler.h>
#include <climax/Atomic.h>
#include <climax/Frame.h>
#include <climax/Physics.h>
#include <climax/unk_801A92A8.h>

/*
 * A physics-driven object of the scene: a clump whose atomic is driven by a rigid body of the
 * physics manager. Registered by name as "CPhysicsObject" (string literal in the factory
 * fn_800A4EEC's registration, unit 0x800A40B4-0x800A4F64). Size 0x1EC; the base unk_8009D2E4 is
 * 0x144 bytes. Field names other than the ones the code shows are not yet recovered.
 *
 * Status: the class layout is provisional. The virtual slots of its vtable (lbl_803E92E0) are
 * not declared yet, so the functions below are free functions taking the object, and the
 * constructor and destructor are not written.
 */
struct unk_RpGeometry;

class CPhysicsObject : public unk_8009D2E4 {
public:
    unk_801A92A8    field_0x144;        // 0x144, message id of the constructor's first registration
    unk_801A92A8    field_0x14C;        // 0x14C, message id of the second registration
    u8              field_0x154;        // 0x154, set once the frame message is registered (fn_800A4380)
    u8              field_0x155;        // 0x155, enables the frame update (set by the constructor)
    u8              field_0x156[0x2];
    unk_RpAtomic*   atomic;             // 0x158, the clump's atomic (NULL when the clump has none)
    unk_RwpBody*    body;               // 0x15C, the rigid body (NULL while there is none)
    unk_RwMatrix    inverseMatrix;      // 0x160, inverse of the body's matrix (RwMatrixInvert)
    u8              field_0x1A0[0x4];
    float           field_0x1A4;        // 0x1A4, set from the geometry's 0x28 word (a radius)
    float           extent[3];          // 0x1A8, the geometry's bounding box size
    float           halfExtentSq[3];    // 0x1B4, squares of half the scale (fn_800A4380) or of the extent
    float           field_0x1C0[3];     // 0x1C0, the scale vector that fn_800A4380 halves
    u8              field_0x1CC[0xC];
    u8              field_0x1D8;        // 0x1D8, selects the matrix-based extents
    u8              field_0x1D9;        // 0x1D9, set when the clump has no geometry
    u8              field_0x1DA;        // 0x1DA, cleared by the constructor
    u8              field_0x1DB;
    u32             field_0x1DC;        // 0x1DC, zero from the constructor
    unk_RpGeometry* geometry;           // 0x1E0, the atomic's geometry (RpGeometryGetData)
    CPhysicsObject* nextObject;         // 0x1E4, link of the static list headed by lbl_80529800
    u8              field_0x1E8;        // 0x1E8, cleared by the constructor
    u8              field_0x1E9[0x3];
};

// the head of the list of CPhysicsObject instances (.sbss, defined in this unit)
extern CPhysicsObject* lbl_80529800;

extern "C" {
void fn_800A40B4();
void fn_800A4204();
void fn_800A437C();
void fn_800A4380( CPhysicsObject* self );
void fn_800A4470( CPhysicsObject* self );
void fn_800A4528( CPhysicsObject* self );
void fn_800A494C();
void fn_800A4C54();
void fn_800A4DF4();
void fn_800A4EEC();
void fn_800A4F34();
void fn_800A4F3C();
void fn_800A4F44();
void fn_800A4F4C();
void fn_800A4F54();
void fn_800A4F5C();
}

#endif
