// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <climax/unk_8007C9E0.h>
#include <game/unk_800276E0.h>
#include <revolution/gx/GXTransform.h>

unk_RwMatrix lbl_80469F30;

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E78C8 size 0xD8 lbl_803E78C8
//   .data 0x803E79A0 size 0x7C lbl_803E79A0
//   .data 0x803E7A1C size 0xB lbl_803E7A1C
//   .data 0x803E7A28 size 0xB lbl_803E7A28
//   .bss 0x80469F20 size 0x10 lbl_80469F20
//   .bss 0x80469F30 size 0x50 lbl_80469F30
//   .bss 0x80469F80 size 0x10 lbl_80469F80
//   .sbss 0x805295D0 size 0x4 lbl_805295D0
//   .sbss 0x805295D4 size 0x4 lbl_805295D4
//   .sbss 0x805295D8 size 0x4 lbl_805295D8
//   .sbss 0x805295DC size 0x4 lbl_805295DC
//   .sbss 0x805295E0 size 0x4 lbl_805295E0
//   .sbss 0x805295E4 size 0x1 lbl_805295E4
//   .sbss 0x805295E8 size 0x4 lbl_805295E8
//   .sbss 0x805295EC size 0x1 lbl_805295EC
//   .sbss 0x805295ED size 0x1 lbl_805295ED
//   .sbss 0x805295F0 size 0x4 lbl_805295F0
//   .sbss 0x805295F4 size 0x1 lbl_805295F4
//   .sbss 0x805295F8 size 0x4 lbl_805295F8
//   .sbss 0x805295FC size 0x4 lbl_805295FC
//   .sbss 0x80529600 size 0x8 lbl_80529600
//   .sdata2 0x8052BE50 size 0x4 lbl_8052BE50
//   .sdata2 0x8052BE54 size 0x4 lbl_8052BE54
//   .sdata2 0x8052BE58 size 0x4 lbl_8052BE58
//   .sdata2 0x8052BE5C size 0x4 lbl_8052BE5C
//   .sdata2 0x8052BE60 size 0x4 lbl_8052BE60
//   .sdata2 0x8052BE64 size 0x4 lbl_8052BE64
//   .sdata2 0x8052BE68 size 0x4 lbl_8052BE68
//   .sdata2 0x8052BE6C size 0x4 lbl_8052BE6C
//   .sdata2 0x8052BE70 size 0x4 lbl_8052BE70
//   .sdata2 0x8052BE78 size 0x8 lbl_8052BE78
//   .sdata2 0x8052BE80 size 0x4 lbl_8052BE80
//   .sdata2 0x8052BE84 size 0x4 lbl_8052BE84
//   .sdata2 0x8052BE88 size 0x4 lbl_8052BE88
//   .sdata2 0x8052BE8C size 0x4 lbl_8052BE8C

/*
====================
fn_8007C9E0
====================
*/
// 0x8007C9E0
// Wall: cReflector constructor (retail calls __ct__14CBaseBehaviourFv and builds the textures and atomic); needs the class layout (vptr slots at 0x00, 0x24, 0x2C, 0x34 are not in the header yet).
extern "C" void fn_8007C9E0() {
    // TODO
}

/*
====================
fn_8007CE84
====================
*/
// 0x8007CE84
// Wall: cReflector deleting destructor (retail: four vptr stores, CEventHandler unregistration, texture and atomic release, flag-driven delete); needs the class layout first.
extern "C" void fn_8007CE84() {
    // TODO
}

/*
====================
cReflector::fn_801A7BFC
====================
*/
// 0x8007D020
void cReflector::fn_801A7BFC( CZoneChangeInfo* change ) {
    CAttributeHandler::fn_801A7BFC( change );
    if ( field_0x50 != NULL ) {
        if ( change->oldZone.GetZone() != NULL ) {
            fn_800276E8( change->oldZone.zone, field_0x50 );
            field_0xC4 = NULL;
        }
        if ( change->newZone.zone != NULL && change->newZone.zone != field_0xC4 ) {
            fn_800276E0( change->newZone.zone, field_0x50 );
            field_0xC4 = change->newZone.zone;
        }
    }
}

/*
====================
cReflector::fn_801A7A88
====================
*/
// 0x8007D0A8
void cReflector::fn_801A7A88() {
    CAttributeHandler::fn_801A7A88();
}

/*
====================
cReflector::fn_8009CB1C
====================
*/
// 0x8007D0AC
// Wall: retail keeps the two identical fn_8007D76C arms separate; MWCC merges them here (call set, control flow).
void cReflector::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A858 ) {
        fn_8007D76C( this, &lbl_80491B38 );
        return;
    }
    if ( msg.record == lbl_8052A860 ) {
        fn_8007D76C( this, &lbl_80491B38 );
        return;
    }
    if ( msg.record == field_0xAC ) {
        field_0xA8 = true;
        return;
    }
    if ( msg.record == field_0xB4 ) {
        field_0xA8 = false;
        return;
    }
    if ( msg.record == field_0xBC ) {
        RwMatrixMultiply( RwFrameGetMatrix( field_0x30 ), &field_0x68, static_cast<const unk_RwMatrix*>( msg.data ) );
        RwMatrixUpdate( RwFrameGetMatrix( field_0x30 ) );
        RwFrameUpdateObjects( field_0x30 );
    }
}

/*
====================
cReflector::fn_801A73A8
====================
*/
// 0x8007D18C
void cReflector::fn_801A73A8( const unk_AttributePacket& packet ) {
    // TODO
}

/*
====================
fn_8007D4C0
====================
*/
// 0x8007D4C0
extern "C" int fn_8007D4C0( unk_RwMatrix* matrix ) {
    f32 texMtx[3][4];
    unk_RwMatrix world;
    if ( matrix != NULL ) {
        RwMatrixMultiply( &world, matrix, &lbl_80469F30 );
        RwClimaxRVLConvertMatrix( &world, texMtx );
    } else {
        RwClimaxRVLConvertMatrix( &lbl_80469F30, texMtx );
    }
    GXLoadTexMtxImm( texMtx, 0x33, GX_MTX2x4 );
    return 0;
}

/*
====================
fn_8007D52C
====================
*/
// 0x8007D52C
extern "C" void fn_8007D52C() {
    // TODO
}

/*
====================
fn_8007D698
====================
*/
// 0x8007D698
extern "C" void fn_8007D698() {
    // TODO
}

/*
====================
fn_8007D76C
====================
*/
// 0x8007D76C
extern "C" void fn_8007D76C( CBaseBehaviour* behaviour, unk_80491B38* player ) {
    // TODO
}

/*
====================
fn_8007E56C
====================
*/
// 0x8007E56C
extern "C" void fn_8007E56C() {
    // TODO
}

/*
====================
fn_8007E694
====================
*/
// 0x8007E694
extern "C" void fn_8007E694() {
    // TODO
}

/*
====================
fn_8007E7D4
====================
*/
// 0x8007E7D4
extern "C" void fn_8007E7D4() {
    // TODO
}

/*
====================
RenderState::SetCullMode
====================
*/
// 0x8007EB18
// TODO: signature not expanded or needs a complete type: SetCullMode__11RenderStateFQ211RenderState10E_CullMode
// void RenderState::SetCullMode( RenderState::E_CullMode ) { }

/*
====================
fn_8007EC0C
====================
*/
// 0x8007EC0C
extern "C" void fn_8007EC0C() {
    // TODO
}

/*
====================
fn_8007ED6C
====================
*/
// 0x8007ED6C
extern "C" void fn_8007ED6C() {
    // TODO
}

/*
====================
fn_8007EE60
====================
*/
// 0x8007EE60
extern "C" void fn_8007EE60() {
    // TODO
}

/*
====================
fn_8007EEDC
====================
*/
// 0x8007EEDC
extern "C" void fn_8007EEDC() {
    // TODO
}

/*
====================
fn_8007EFE4

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x8007EFE4
extern "C" void fn_8007EFE4() {
    // TODO
}

/*
====================
fn_8007F02C
====================
*/
// 0x8007F02C
extern "C" void fn_8007F02C() {
    // TODO
}

/*
====================
fn_8007F034
====================
*/
// 0x8007F034
extern "C" void fn_8007F034() {
    // TODO
}

/*
====================
fn_8007F03C
====================
*/
// 0x8007F03C
extern "C" void fn_8007F03C() {
    // TODO
}

// Compiler-generated, no source of their own: fn_8007EF58 (0x8007EF58).
