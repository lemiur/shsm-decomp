// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009D2E4.h>
#include <game/CZoneChangeInfo.h>

// The behaviour whose clump fn_8009D688 is setting up (name invented from the address).
unk_8009D2E4* lbl_805297C4;

// Atomic callback of the clump setup, defined below.
extern "C" unk_RpAtomic* fn_8009EAE0( unk_RpAtomic* atomic, void* data );

// Behaviour setup from an attribute packet (0x8009D520, its body not yet reconstructed).
extern "C" void fn_8009D520( unk_8009D2E4* self, const unk_AttributePacket* packet, bool flag );

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E8C60 size 0x34 jumptable_803E8C60
//   .data 0x803E8C94 size 0xE4 lbl_803E8C94
//   .data 0x803E8D78 size 0xEC lbl_803E8D78
//   .data 0x803E8E64 size 0x4C lbl_803E8E64
//   .sdata 0x805272E8 size 0x8 lbl_805272E8
//   .sbss 0x805297BC size 0x4 lbl_805297BC
//   .sbss 0x805297C0 size 0x4 lbl_805297C0
//   .sbss 0x805297C4 size 0x4 lbl_805297C4
//   .sdata2 0x8052C300 size 0x8 lbl_8052C300

/*
====================
unk_8009D2E4::unk_8009D2E4
====================
*/
// Field setup shared by both constructors (name invented): the base constructors and the
// vtable pointers have already run when the body starts; this zeroes the behaviour's own
// vectors and flags, then runs fn_8009D520 on the attribute packet.
// Wall: NonMatching. Retail calls fn_80024158 for the unk_80024158 base at 0x3C (no declared
// constructor here), keeps unk_803E8E64's inline base stores, and names the vtable lbl_803E8D78
// where this TU emits __vt__12unk_8009D2E4; fn_8009D520 is still a stub.
static inline void InitBehaviourState( unk_8009D2E4* self, const unk_AttributePacket& packet, bool flag ) {
    self->field_0xA8 = 0.0f;
    self->field_0xA4 = 0.0f;
    self->field_0xA0 = 0.0f;
    self->field_0xB4 = 0.0f;
    self->field_0xB0 = 0.0f;
    self->field_0xAC = 0.0f;
    self->field_0xCC = 0.0f;
    self->field_0xC8 = 0.0f;
    self->field_0xC4 = 0.0f;
    self->field_0xD8 = 0.0f;
    self->field_0xD4 = 0.0f;
    self->field_0xD0 = 0.0f;
    self->field_0x120 = 0;
    self->field_0xDC = 1;
    self->field_0x124 = 0.0f;
    self->field_0x128 = 0.0f;
    self->field_0x12C = 0;
    self->field_0x130 = NULL;
    self->field_0x134 = NULL;
    self->field_0x140 = 1;
    self->field_0x142 = 0;
    fn_8009D520( self, &packet, flag );
}

// 0x8009D2E4
unk_8009D2E4::unk_8009D2E4( const unk_AttributePacket& packet ) {
    InitBehaviourState( this, packet, true );
}

/*
====================
unk_8009D2E4::unk_8009D2E4
====================
*/
// 0x8009D41C
unk_8009D2E4::unk_8009D2E4( const unk_AttributePacket& packet, bool flag ) {
    InitBehaviourState( this, packet, flag );
}

/*
====================
fn_8009D520
====================
*/
// 0x8009D520
extern "C" void fn_8009D520( unk_8009D2E4* self, const unk_AttributePacket* packet, bool flag ) {
    // TODO: stub, the body picks the clump from the packet (fn_801AB838 or fn_801AB86C by flag),
    // then walks the zone-side helpers; the callee signatures are not yet recovered.
}

/*
====================
fn_8009D688
====================
*/
// Sets up the clump of a behaviour: adopts the clump it is given while it has none, puts its
// root frame into the modelling setup, and resets every atomic through fn_8009EAE0.
// 0x8009D688
extern "C" void fn_8009D688( unk_8009D2E4* self, unk_RpClump* clump ) {
    if ( self->field_0x130 == NULL && clump != NULL ) {
        self->field_0x130 = clump;
    }
    self->field_0x138 = 0;
    self->field_0x13C = 0;
    if ( self->field_0x130 != NULL ) {
        fn_8009C838( self, RpClumpGetFrame( self->field_0x130 ) );
        RwFrameSetIdentity( self->field_0x30 );
        lbl_805297C4 = self;
        RpClumpForAllAtomics( self->field_0x130, fn_8009EAE0, NULL );
        lbl_805297C4 = NULL;
    }
}

/*
====================
unk_8009D2E4::~unk_8009D2E4
====================
*/
// 0x8009D718
unk_8009D2E4::~unk_8009D2E4() {
    // TODO
}

/*
====================
unk_8009D2E4::fn_801A7BFC
====================
*/
// 0x8009D840
void unk_8009D2E4::fn_801A7BFC( CZoneChangeInfo* change ) {
    CAttributeHandler::fn_801A7BFC( change );
    if ( field_0x130 != NULL ) {
        if ( change->oldZone.GetZone() != NULL ) {
            fn_80027740( change->oldZone.zone, field_0x130 );
        }
        if ( change->newZone.GetZone() != NULL ) {
            fn_80027738( change->newZone.zone, field_0x130 );
        }
    }
}

/*
====================
unk_8009D2E4::fn_8009CB1C
====================
*/
// 0x8009D8B0
void unk_8009D2E4::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    unk_80024158::fn_8009CB1C( msg );
}

/*
====================
fn_8009D8F4
====================
*/
// 0x8009D8F4
extern "C" void fn_8009D8F4() {
    // TODO
}

/*
====================
fn_8009E3AC
====================
*/
// 0x8009E3AC
extern "C" void fn_8009E3AC() {
    // TODO
}

/*
====================
fn_8009E50C
====================
*/
// 0x8009E50C
extern "C" void fn_8009E50C() {
    // TODO
}

/*
====================
fn_8009E544
====================
*/
// 0x8009E544
extern "C" void fn_8009E544() {
    // TODO
}

/*
====================
fn_8009E598
====================
*/
// 0x8009E598
extern "C" void fn_8009E598() {
    // TODO
}

/*
====================
unk_8009D2E4::fn_801A73A8
====================
*/
// 0x8009E5D4
void unk_8009D2E4::fn_801A73A8( const unk_AttributePacket& ) {
    // TODO
}

/*
====================
unk_8009D2E4::fn_8009E6D4
====================
*/
// 0x8009E6D4
void unk_8009D2E4::fn_8009E6D4() {
    // TODO
}

/*
====================
fn_8009E7C4
====================
*/
// 0x8009E7C4
extern "C" void fn_8009E7C4() {
    // TODO
}

/*
====================
fn_8009E88C
====================
*/
// 0x8009E88C
extern "C" void fn_8009E88C() {
    // TODO
}

/*
====================
fn_8009E894
====================
*/
// 0x8009E894
extern "C" void fn_8009E894() {
    // TODO
}

/*
====================
fn_8009E8F8
====================
*/
// 0x8009E8F8
extern "C" void fn_8009E8F8() {
    // TODO
}

/*
====================
fn_8009E944
====================
*/
// 0x8009E944
extern "C" void fn_8009E944() {
    // TODO
}

/*
====================
fn_8009E980
====================
*/
// 0x8009E980
extern "C" void fn_8009E980() {
    // TODO
}

/*
====================
fn_8009EAE0
====================
*/
// 0x8009EAE0
extern "C" unk_RpAtomic* fn_8009EAE0( unk_RpAtomic* atomic, void* data ) {
    // TODO
    return atomic;
}

/*
====================
fn_8009EB2C
====================
*/
// 0x8009EB2C
extern "C" void fn_8009EB2C() {
    // TODO
}

/*
====================
unk_8009D2E4::fn_8009EB8C
====================
*/
// 0x8009EB8C
void unk_8009D2E4::fn_8009EB8C() {
    // TODO
}

/*
====================
unk_8009D2E4::fn_8009EC0C
====================
*/
// 0x8009EC0C
void unk_8009D2E4::fn_8009EC0C() {
    // TODO
}

/*
====================
fn_8009EC5C
====================
*/
// 0x8009EC5C
extern "C" void fn_8009EC5C() {
    // TODO
}

/*
====================
fn_8009ECB0
====================
*/
// 0x8009ECB0
extern "C" void fn_8009ECB0() {
    // TODO
}
