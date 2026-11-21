#include <game/CPhysicsObject.h>
#include <game/Messages.h>
#include <game/unk_PhysicsManager.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E92A8 size 0xB lbl_803E92A8
//   .data 0x803E92B4 size 0x2C jumptable_803E92B4
//   .data 0x803E92E0 size 0xF8 lbl_803E92E0
//   .data 0x803E93D8 size 0xF lbl_803E93D8
//   .data 0x803E93E8 size 0xF lbl_803E93E8
//   .bss 0x8046E8A0 size 0x10 lbl_8046E8A0
//   .sbss 0x805297F8 size 0x4 lbl_805297F8
//   .sbss 0x805297FC size 0x4 lbl_805297FC
//   .sbss 0x80529800 size 0x8 lbl_80529800
//   .sdata2 0x8052C340 size 0x4 lbl_8052C340
//   .sdata2 0x8052C344 size 0x4 lbl_8052C344
//   .sdata2 0x8052C348 size 0x4 lbl_8052C348
//   .sdata2 0x8052C34C size 0x4 lbl_8052C34C
//   .sdata2 0x8052C350 size 0x8 lbl_8052C350

/*
====================
fn_800A40B4
====================
*/
// 0x800A40B4
extern "C" void fn_800A40B4() {
    // TODO
}

/*
====================
fn_800A4204
====================
*/
// 0x800A4204
extern "C" void fn_800A4204() {
    // TODO
}

/*
====================
fn_800A437C
====================
*/
// 0x800A437C
extern "C" void fn_800A437C() {
    // TODO
}

/*
====================
fn_800A4380
====================
*/
// Not yet exact: the call to fn_800A4528 is dropped while that function is an empty stub (the
// compiler inlines it); the rest of the body matches.
// 0x800A4380
extern "C" void fn_800A4380( CPhysicsObject* self ) {
    if ( self->field_0x155 == 0 ) {
        return;
    }
    if ( self->field_0x154 != 0 ) {
        return;
    }
    fn_800A4528( self );
    if ( self->field_0x1D8 != 0 ) {
        RwMatrixInvert( &self->inverseMatrix, self->body->parts->matrix );
        if ( self->field_0x1D9 != 0 ) {
            for ( int i = 0; i < 3; i++ ) {
                float half = 0.5f * self->field_0x1C0[i];
                self->halfExtentSq[i] = half * half;
            }
        } else {
            for ( int i = 0; i < 3; i++ ) {
                self->halfExtentSq[i] = self->extent[i] * self->extent[i];
            }
        }
    }
    self->fn_801A9DE8( lbl_8052A788 );
    self->field_0x154 = 1;
}

/*
====================
fn_800A4470
====================
*/
// 0x800A4470
extern "C" void fn_800A4470( CPhysicsObject* self ) {
    if ( self->field_0x154 == 0 ) {
        return;
    }
    if ( self->body != NULL ) {
        lbl_80529170->fn_8000F71C( self->body );
        if ( self->atomic != NULL ) {
            RpAtomicSetBody( self->atomic, NULL );
        }
        fn_802FA9D4( self->body );
        self->body = NULL;
    }
    if ( self->atomic != NULL ) {
        unk_RwFrame* frame = ( unk_RwFrame* )self->atomic->parent;
        RpAtomicSetFrame( self->atomic, NULL );
        RwFrameDestroy( frame );
        RpAtomicDestroy( self->atomic );
        self->atomic = NULL;
    }
    self->fn_801A9E00( lbl_8052A788 );
    self->field_0x154 = 0;
}

/*
====================
fn_800A4528
====================
*/
// Not written: the body needs two fields of the base unk_8009D2E4 that no declared base class
// names yet (a word stored as 4 at 0x38, and the owner chain read from 0xC, then its 0xC0 and
// 0x0, which is the physics space). Its reads of 0x30 (the frame), 0x1C0-0x1D8 and the
// RwpBodyDef calls (0x802FA724, 0x802FA91C, 0x802FA7AC, 0x80321220, 0x8030CD48) are known.
// 0x800A4528
extern "C" void fn_800A4528( CPhysicsObject* self ) {
    // TODO
}

/*
====================
fn_800A494C
====================
*/
// 0x800A494C
extern "C" void fn_800A494C() {
    // TODO
}

/*
====================
fn_800A4C54
====================
*/
// 0x800A4C54
extern "C" void fn_800A4C54() {
    // TODO
}

/*
====================
fn_800A4DF4
====================
*/
// 0x800A4DF4
extern "C" void fn_800A4DF4() {
    // TODO
}

/*
====================
fn_800A4EEC

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x800A4EEC
extern "C" void fn_800A4EEC() {
    // TODO
}

/*
====================
fn_800A4F34
====================
*/
// 0x800A4F34
extern "C" void fn_800A4F34() {
    // TODO
}

/*
====================
fn_800A4F3C
====================
*/
// 0x800A4F3C
extern "C" void fn_800A4F3C() {
    // TODO
}

/*
====================
fn_800A4F44
====================
*/
// 0x800A4F44
extern "C" void fn_800A4F44() {
    // TODO
}

/*
====================
fn_800A4F4C
====================
*/
// 0x800A4F4C
extern "C" void fn_800A4F4C() {
    // TODO
}

/*
====================
fn_800A4F54
====================
*/
// 0x800A4F54
extern "C" void fn_800A4F54() {
    // TODO
}

/*
====================
fn_800A4F5C
====================
*/
// 0x800A4F5C
extern "C" void fn_800A4F5C() {
    // TODO
}

// Compiler-generated, no source of their own: fn_800A4E8C (0x800A4E8C).
