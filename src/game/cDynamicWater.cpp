// Unit name unconfirmed: the unit holds the water and surface classes registered in fn_80084BC0 (cDynamicWater, cPlaneWater, cUnderWater, cSurfaceDetector, cFloatObject); the file is named after the first one

#include <game/cDynamicWater.h>
#include <rwsdk/batextur.h>

cDynamicWater* lbl_80529640;

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E7A50 size 0x9 lbl_803E7A50
//   .data 0x803E7A5C size 0x9 lbl_803E7A5C
//   .data 0x803E7A68 size 0x30 jumptable_803E7A68
//   .data 0x803E7A98 size 0x16 lbl_803E7A98
//   .data 0x803E7AB0 size 0x3C jumptable_803E7AB0
//   .data 0x803E7AEC size 0x9 lbl_803E7AEC
//   .data 0x803E7AF8 size 0x58 lbl_803E7AF8
//   .data 0x803E7B50 size 0x58 lbl_803E7B50
//   .data 0x803E7BA8 size 0x5C lbl_803E7BA8
//   .data 0x803E7C04 size 0x5C lbl_803E7C04
//   .data 0x803E7C60 size 0x118 lbl_803E7C60
//   .bss 0x80469F90 size 0x70 lbl_80469F90
//   .sbss 0x80529610 size 0x4 lbl_80529610
//   .sbss 0x80529614 size 0x4 lbl_80529614
//   .sbss 0x80529618 size 0x4 lbl_80529618
//   .sbss 0x8052961C size 0x4 lbl_8052961C
//   .sbss 0x80529620 size 0x4 lbl_80529620
//   .sbss 0x80529624 size 0x4 lbl_80529624
//   .sbss 0x80529628 size 0x4 lbl_80529628
//   .sbss 0x8052962C size 0x4 lbl_8052962C
//   .sbss 0x80529630 size 0x4 lbl_80529630
//   .sbss 0x80529634 size 0x4 lbl_80529634
//   .sbss 0x80529638 size 0x4 lbl_80529638
//   .sbss 0x8052963C size 0x4 lbl_8052963C
//   .sbss 0x80529640 size 0x4 lbl_80529640
//   .sbss 0x80529644 size 0x4 lbl_80529644
//   .sdata2 0x8052BEE8 size 0x4 lbl_8052BEE8
//   .sdata2 0x8052BEEC size 0x4 lbl_8052BEEC
//   .sdata2 0x8052BEF0 size 0x4 lbl_8052BEF0
//   .sdata2 0x8052BEF4 size 0x4 lbl_8052BEF4
//   .sdata2 0x8052BEF8 size 0x4 lbl_8052BEF8
//   .sdata2 0x8052BEFC size 0x4 lbl_8052BEFC
//   .sdata2 0x8052BF00 size 0x4 lbl_8052BF00
//   .sdata2 0x8052BF04 size 0x4 lbl_8052BF04
//   .sdata2 0x8052BF08 size 0x4 lbl_8052BF08
//   .sdata2 0x8052BF0C size 0x4 lbl_8052BF0C
//   .sdata2 0x8052BF10 size 0x4 lbl_8052BF10
//   .sdata2 0x8052BF14 size 0x4 lbl_8052BF14
//   .sdata2 0x8052BF18 size 0x4 lbl_8052BF18
//   .sdata2 0x8052BF1C size 0x4 lbl_8052BF1C
//   ... 29 more

/*
====================
fn_80081928
====================
*/
// 0x80081928
extern "C" void fn_80081928() {
    // TODO
}

/*
====================
__dt__13cDynamicWaterFv
====================
*/
// 0x80081E00
// Not exact: retail also stores a vptr at 0x34 (a polymorphic base not yet declared), and it names the vtable lbl_803E7C60, not __vt__13cDynamicWater.
cDynamicWater::~cDynamicWater() {
    if ( lbl_80529640 == this ) {
        lbl_80529640 = NULL;
    }
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( field_0x19c );
    fn_801A9E00( field_0x1a4 );
    fn_801A9940( field_0x19c );
    fn_801A9940( field_0x1a4 );
    if ( field_0x74 != NULL ) {
        RpAtomicSetFrame( field_0x74, NULL );
        RpAtomicDestroy( field_0x74 );
    }
    fn_801A9940( field_0x18c );
    fn_801A9940( field_0x194 );
    delete[] field_0x1ac;
    if ( field_0x48 != NULL ) {
        RwTextureDestroy( field_0x48 );
    }
    if ( field_0x54 != NULL ) {
        RwTextureDestroy( field_0x54 );
    }
    if ( field_0x60 != NULL ) {
        RwTextureDestroy( field_0x60 );
    }
    if ( field_0x3c != NULL ) {
        RwTextureDestroy( field_0x3c );
    }
}

/*
====================
cDynamicWater::fn_801A7BFC
====================
*/
// 0x80081F6C
void cDynamicWater::fn_801A7BFC( CZoneChangeInfo* change ) {
    CAttributeHandler::fn_801A7BFC( change );
    if ( field_0x74 != NULL ) {
        if ( change->oldZone.GetZone() ) {
            fn_800276E8( change->oldZone.zone, field_0x74 );
        }
        if ( change->newZone.GetZone() != NULL ) {
            fn_800276E0( change->newZone.zone, field_0x74 );
        }
    }
}

/*
====================
cDynamicWater::fn_8009CB1C
====================
*/
// 0x80081FDC
void cDynamicWater::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788.record ) {
        fn_80082D3C( this, &lbl_80491B38 );
    } else if ( msg.record == field_0x19c.record ) {
        RwMatrixMultiply( RwFrameGetMatrix( field_0x30 ), &field_0xf8, (unk_RwMatrix*)msg.data );
        RwMatrixInvert( &field_0x78, RwFrameGetMatrix( field_0x30 ) );
        RwFrameUpdateObjects( field_0x30 );
    } else if ( msg.record == field_0x1a4.record ) {
        unk_RwMatrix* matrix = (unk_RwMatrix*)msg.data;
        field_0x138 = *matrix;
        RwMatrixMultiply( &field_0xb8, matrix, &field_0x78 );
    }
}

/*
====================
fn_80082110
====================
*/
// 0x80082110
extern "C" void fn_80082110() {
    // TODO
}

/*
====================
fn_80082488
====================
*/
// 0x80082488
extern "C" void fn_80082488() {
    // TODO
}

/*
====================
fn_80082588
====================
*/
// 0x80082588
extern "C" void fn_80082588() {
    // TODO
}

/*
====================
fn_8008288C
====================
*/
// 0x8008288C
extern "C" void fn_8008288C() {
    // TODO
}

/*
====================
fn_80082B70
====================
*/
// 0x80082B70
extern "C" void fn_80082B70() {
    // TODO
}

/*
====================
fn_80082D3C
====================
*/
// 0x80082D3C
// The stub body is empty, so the optimiser would drop the calls to it; the call stays until the body is reconstructed.
#pragma dont_inline on
extern "C" void fn_80082D3C( cDynamicWater* self, const void* hit ) {
    // TODO
}
#pragma dont_inline reset

/*
====================
fn_80083250
====================
*/
// 0x80083250
extern "C" void fn_80083250() {
    // TODO
}

/*
====================
fn_80083318
====================
*/
// 0x80083318
extern "C" void fn_80083318() {
    // TODO
}

/*
====================
fn_800833D8
====================
*/
// 0x800833D8
extern "C" void fn_800833D8() {
    // TODO
}

/*
====================
fn_800834E8
====================
*/
// 0x800834E8
extern "C" void fn_800834E8() {
    // TODO
}

/*
====================
fn_80083580
====================
*/
// 0x80083580
extern "C" void fn_80083580() {
    // TODO
}

/*
====================
fn_80083688
====================
*/
// 0x80083688
extern "C" void fn_80083688() {
    // TODO
}

/*
====================
fn_800836D8
====================
*/
// 0x800836D8
extern "C" void fn_800836D8() {
    // TODO
}

/*
====================
fn_8008393C
====================
*/
// 0x8008393C
extern "C" void fn_8008393C() {
    // TODO
}

/*
====================
fn_80083A60
====================
*/
// 0x80083A60
extern "C" void fn_80083A60() {
    // TODO
}

/*
====================
fn_80083BD4
====================
*/
// 0x80083BD4
extern "C" void fn_80083BD4() {
    // TODO
}

/*
====================
fn_80083C38
====================
*/
// 0x80083C38
extern "C" void fn_80083C38() {
    // TODO
}

/*
====================
fn_80083EC0
====================
*/
// 0x80083EC0
extern "C" void fn_80083EC0() {
    // TODO
}

/*
====================
fn_80084060
====================
*/
// 0x80084060
extern "C" void fn_80084060() {
    // TODO
}

/*
====================
fn_8008413C
====================
*/
// 0x8008413C
extern "C" void fn_8008413C() {
    // TODO
}

/*
====================
fn_8008423C
====================
*/
// 0x8008423C
extern "C" void fn_8008423C() {
    // TODO
}

/*
====================
fn_80084404
====================
*/
// 0x80084404
extern "C" void fn_80084404() {
    // TODO
}

/*
====================
fn_80084678
====================
*/
// 0x80084678
extern "C" void fn_80084678() {
    // TODO
}

/*
====================
fn_80084828
====================
*/
// 0x80084828
extern "C" void fn_80084828() {
    // TODO
}

/*
====================
fn_800848F0
====================
*/
// 0x800848F0
extern "C" void fn_800848F0() {
    // TODO
}

/*
====================
fn_80084A5C
====================
*/
// 0x80084A5C
extern "C" void fn_80084A5C() {
    // TODO
}

/*
====================
fn_80084D0C

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x80084D0C
extern "C" void fn_80084D0C() {
    // TODO
}

/*
====================
fn_80084D54

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x80084D54
extern "C" void fn_80084D54() {
    // TODO
}

/*
====================
fn_80084D9C

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x80084D9C
extern "C" void fn_80084D9C() {
    // TODO
}

/*
====================
fn_80084DE4

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x80084DE4
extern "C" void fn_80084DE4() {
    // TODO
}

/*
====================
fn_80084E98

Address taken by the static initialiser: probably an inline factory that belongs in the header.
====================
*/
// 0x80084E98
extern "C" void fn_80084E98() {
    // TODO
}

/*
====================
fn_80084F38
====================
*/
// 0x80084F38
extern "C" void fn_80084F38() {
    // TODO
}

/*
====================
fn_80084F40
====================
*/
// 0x80084F40
extern "C" void fn_80084F40() {
    // TODO
}

/*
====================
fn_80084F48
====================
*/
// 0x80084F48
extern "C" void fn_80084F48() {
    // TODO
}

/*
====================
fn_80084F50
====================
*/
// 0x80084F50
extern "C" void fn_80084F50() {
    // TODO
}

/*
====================
fn_80084F58
====================
*/
// 0x80084F58
extern "C" void fn_80084F58() {
    // TODO
}

/*
====================
fn_80084F60
====================
*/
// 0x80084F60
extern "C" void fn_80084F60() {
    // TODO
}

/*
====================
fn_80084F68
====================
*/
// 0x80084F68
extern "C" void fn_80084F68() {
    // TODO
}

/*
====================
fn_80084F70
====================
*/
// 0x80084F70
extern "C" void fn_80084F70() {
    // TODO
}

/*
====================
fn_80084F78
====================
*/
// 0x80084F78
extern "C" void fn_80084F78() {
    // TODO
}

/*
====================
fn_80084F80
====================
*/
// 0x80084F80
extern "C" void fn_80084F80() {
    // TODO
}

/*
====================
fn_80084F88
====================
*/
// 0x80084F88
extern "C" void fn_80084F88() {
    // TODO
}

// Compiler-generated, no source of their own: fn_80084BC0 (0x80084BC0).
