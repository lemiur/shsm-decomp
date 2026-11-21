// Unit name and boundary: class registry string "cIce" (fn_80087D88), factory fn_80087E0C, and
// the destructor and vtable lbl_803E80D8 in this unit (classes.exe evidence).

#include <game/cIce.h>
#include <climax/Frame.h>



u8 lbl_8052969D;

/*
====================
cIce::~cIce
Destroys the ice's clump, removing its frame from the parent first. The base destructor and the
vptr stores are the compiler's.
Open: the vtable pointer at 0x18 (the override of CAttributeHandler::fn_801A7A88 in the secondary
base) is not placed as in retail; the bytes match, the vtable relocation target does not.
====================
*/
// 0x800878D0
cIce::~cIce() {
    if ( field_0x144 != NULL ) {
        RwFrameRemoveChild( RpClumpGetFrame( field_0x144 ) );
        RpClumpDestroy( field_0x144 );
    }
}

/*
====================
cIce::fn_80087974
Override of the zone-change slot: runs the base, then moves the ice's clump from the world of the
zone left to the world of the zone entered.
Open: the CZone member at 0xA4 is not typed yet (CZone unit not reconstructed), so the world read
goes through CZoneGetWorld, a call retail does not make; the match waits on that referent.
====================
*/
// 0x80087974
void cIce::fn_80087974( CZoneChangeInfo* change ) {
    unk_8009D2E4::fn_801A7BFC( change );
    if ( field_0x144 != NULL ) {
        if ( change->oldZone.GetZone() != NULL ) {
            RpWorld* world = CZoneGetWorld( change->oldZone.GetZone() );
            if ( world != NULL ) {
                RpWorldRemoveClump( world, field_0x144 );
            }
        }
        if ( change->newZone.GetZone() != NULL ) {
            RpWorld* world = CZoneGetWorld( change->newZone.GetZone() );
            if ( world != NULL ) {
                RpWorldAddClump( world, field_0x144 );
            }
        }
    }
}

/*
====================
fn_800879FC
Builds a view matrix from the GX projection (the scale and offset of the refraction texture space)
and multiplies it by the inverse camera matrix into lbl_8046A510.
====================
*/
// 0x800879FC
void fn_800879FC() {
    f32 negHalf = -0.5f;
    f32 half = 0.5f;
    f32 proj[7];
    unk_RwMatrix view;

    GXGetProjectionv( proj );
    view.right.x = proj[1] * half;
    view.right.y = 0.0f;
    view.right.z = 0.0f;
    view.up.x = 0.0f;
    view.up.y = proj[3] * half;
    view.up.z = 0.0f;
    view.at.x = proj[2] + negHalf;
    view.at.y = proj[4] + negHalf;
    view.at.z = -1.0f;
    view.pos.x = 0.0f;
    view.pos.y = 0.0f;
    view.pos.z = 0.0f;
    view.flags = 0;
    RwMatrixMultiply( &lbl_8046A510, DisplayListInvCamLTM(), &view );
}

/*
====================
fn_80087AC0
Texture-matrix callback of the refraction handle: multiplies the given matrix by lbl_8046A510 (when
there is one), converts the result to a GX 3x4 matrix and loads it into texture matrix 6.
====================
*/
// 0x80087AC0
unsigned int fn_80087AC0( unk_RwMatrix* ltm, int normals, float ( *ModelView )[4] ) {
    Mtx tex;

    if ( ltm != NULL ) {
        unk_RwMatrix product;
        RwMatrixMultiply( &product, ltm, &lbl_8046A510 );
        RwClimaxRVLConvertMatrix( &product, tex );
    } else {
        RwClimaxRVLConvertMatrix( &lbl_8046A510, tex );
    }
    GXLoadTexMtxImm( tex, GX_TEXMTX6, GX_MTX3x4 );
    return 0;
}

/*
====================
cIce::fn_80087D78
Override of the behaviour's message handler: forwards to the base.
====================
*/
// 0x80087D78
void cIce::fn_80087D78( unk_Msg& msg ) {
    unk_8009D2E4::fn_8009CB1C( msg );
}

/*
====================
cIce::fn_80087D7C
Override of the attribute packet slot: forwards to the base.
====================
*/
// 0x80087D7C
void cIce::fn_80087D7C( const unk_AttributePacket& packet ) {
    unk_8009D2E4::fn_801A73A8( packet );
}

/*
====================
cIce::fn_80087D14
Overrides the attribute handler's update: runs the base, then sets the render callbacks of the
ice's atomics (the clump at 0x130 and the one at 0x144).
====================
*/
// 0x80087D14
void cIce::fn_80087D14() {
    CAttributeHandler::fn_801A7A88();
    if ( field_0x130 != NULL ) {
        RpClumpForAllAtomics( field_0x130, fn_80087C74, this );
    }
    if ( field_0x144 != NULL ) {
        RpClumpForAllAtomics( field_0x144, fn_80087CF4, this );
    }
}

/*
====================
fn_80087BB4
Render callback: runs the default atomic render and returns the atomic.
====================
*/
// 0x80087BB4
unk_RpAtomic* fn_80087BB4( unk_RpAtomic* atomic ) {
    AtomicDefaultRenderCallBack( atomic );
    return atomic;
}

/*
====================
fn_80087CD0
Sets the atomic's render callback to fn_80087BE4 (or the default), then sets the pass.
====================
*/
// 0x80087CD0
void fn_80087CD0( unk_RpAtomic* atomic ) {
    RpAtomicSetRenderCallBack( atomic, fn_80087BE4 );
    RwClimaxAtomicSetPass( atomic, 0x42 );
}

/*
====================
fn_80087CF4
Clump callback: sets the atomic's render callback to fn_80087BB4 (or the default); returns the atomic.
====================
*/
// 0x80087CF4
unk_RpAtomic* fn_80087CF4( unk_RpAtomic* atomic, void* data ) {
    RpAtomicSetRenderCallBack( atomic, fn_80087BB4 );
    return atomic;
}

/*
====================
fn_80087C74
Clump callback: sets the atomic's render callback to fn_80087B2C (or the default), sets the pass,
runs the pass setup and returns the atomic.
====================
*/
// 0x80087C74
unk_RpAtomic* fn_80087C74( unk_RpAtomic* atomic, void* data ) {
    RpAtomicSetRenderCallBack( atomic, fn_80087B2C );
    RwClimaxAtomicSetPass( atomic, 0x42 );
    fn_800BE62C( atomic );
    return atomic;
}

// 0x80087D80
void fn_80087D80( u8 flag ) {
    lbl_8052969D = flag;
}

// Not reconstructed in this pass: fn_80087B2C and fn_80087BE4 (their refraction handle lbl_8046A500 is a
// cTevCBHandle, and TevCBMgr.h clashes with climax/Frame.h at rwCOMBINEREPLACE), and the static
// initialiser fn_80087D88, which builds that same cTevCBHandle at lbl_8046A500 (the same clash).
// The factory fn_80087E0C is the inline static member of cIce.h; the fn_80087E.. thunks are the
// compiler's secondary-base thunks.
