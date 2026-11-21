// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/CPlayer.h>
#include <game/unk_803E5EE0.h>
#include <game/unk_803E5E20.h>
#include <game/unk_803E5E78.h>

// The global unk_803E5EE0's constructor stores the new behaviour into (its readers are not yet recovered).
CBaseBehaviour* lbl_80529568;

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E5EE0 size 0x58 __vt__12unk_803E5EE0
//   .bss 0x80469BF8 size 0x2D0 lbl_80469BF8
//   .sdata 0x80527058 size 0x8 lbl_80527058
//   .sdata 0x80527060 size 0x8 lbl_80527060
//   .sbss 0x80529378 size 0x4 lbl_80529378
//   .sbss 0x8052937C size 0x4 lbl_8052937C
//   .sbss 0x80529380 size 0x1 lbl_80529380
//   .sbss 0x80529381 size 0x7 lbl_80529381
//   .sbss 0x80529388 size 0x8 lbl_80529388
//   .sbss 0x80529390 size 0x8 lbl_80529390
//   .sbss 0x80529398 size 0x8 lbl_80529398
//   .sbss 0x805293A0 size 0x8 lbl_805293A0
//   .sbss 0x805293A8 size 0x8 lbl_805293A8
//   .sbss 0x805293B0 size 0x8 lbl_805293B0
//   .sbss 0x805293B8 size 0x8 lbl_805293B8
//   .sbss 0x805293C0 size 0x8 lbl_805293C0
//   .sbss 0x805293C8 size 0x8 lbl_805293C8
//   .sbss 0x805293D0 size 0x8 lbl_805293D0
//   .sbss 0x805293D8 size 0x8 lbl_805293D8
//   .sbss 0x805293E0 size 0x8 lbl_805293E0
//   .sbss 0x805293E8 size 0x8 lbl_805293E8
//   .sbss 0x805293F0 size 0x8 lbl_805293F0
//   .sbss 0x805293F8 size 0x8 lbl_805293F8
//   .sbss 0x80529400 size 0x8 lbl_80529400
//   .sbss 0x80529408 size 0x8 lbl_80529408
//   .sbss 0x80529410 size 0x8 lbl_80529410
//   .sbss 0x80529418 size 0x8 lbl_80529418
//   .sbss 0x80529420 size 0x8 lbl_80529420
//   .sbss 0x80529428 size 0x8 lbl_80529428
//   .sbss 0x80529430 size 0x8 lbl_80529430
//   .sbss 0x80529438 size 0x8 lbl_80529438
//   .sbss 0x80529440 size 0x8 lbl_80529440
//   .sbss 0x80529448 size 0x8 lbl_80529448
//   .sbss 0x80529450 size 0x8 lbl_80529450
//   .sbss 0x80529458 size 0x8 lbl_80529458
//   .sbss 0x80529460 size 0x8 lbl_80529460
//   .sbss 0x80529468 size 0x8 lbl_80529468
//   .sbss 0x80529470 size 0x8 lbl_80529470
//   .sbss 0x80529478 size 0x8 lbl_80529478
//   .sbss 0x80529480 size 0x8 lbl_80529480
//   ... 37 more

/*
====================
CPlayer::fn_80073BDC

The registered factory: allocates a CPlayer from the game heap and constructs it from the packet.
====================
*/
// 0x80073BDC
CAttributeHandler* CPlayer::fn_80073BDC( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPlayer( packet );
}

/*
====================
fn_80073C24

Factory of unk_803E5EE0 (a CBaseBehaviour with no fields of its own); its constructor keeps the new
object in lbl_80529568.
====================
*/
// 0x80073C24
unk_803E5EE0* fn_80073C24() {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_803E5EE0();
}

/*
====================
fn_80073C8C

Factory of unk_803E5E78 (a CBaseBehaviour with a second interface at 0x34 and a flag, an int and a
unk_801A92A8 member).
====================
*/
// 0x80073C8C
unk_803E5E78* fn_80073C8C() {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_803E5E78();
}

/*
====================
fn_80073D28

Factory of unk_803E5E20 (a CBaseBehaviour with a unk_801A92A8 member at 0x34).
====================
*/
// 0x80073D28
unk_803E5E20* fn_80073D28() {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_803E5E20();
}

/*
====================
fn_80073DA4
Open: compiler thunk (this -= 0x24, tail call fn_800576C0); the TU of CPlayer's vtable is not recovered.
====================
*/
// 0x80073DA4
extern "C" void fn_80073DA4() {
    // TODO
}

/*
====================
fn_80073DAC
Open: compiler thunk (this -= 0x24, tail call fn_80056ADC, the destructor); the TU of CPlayer's vtable is not recovered.
====================
*/
// 0x80073DAC
extern "C" void fn_80073DAC() {
    // TODO
}

/*
====================
fn_80073DB4
Open: compiler thunk (this -= 0x34, tail call fn_80060458); the TU of CPlayer's vtable is not recovered.
====================
*/
// 0x80073DB4
extern "C" void fn_80073DB4() {
    // TODO
}

/*
====================
fn_80073DBC
Open: compiler thunk (this -= 0x34, tail call fn_8006E508); the TU of CPlayer's vtable is not recovered.
====================
*/
// 0x80073DBC
extern "C" void fn_80073DBC() {
    // TODO
}

/*
====================
fn_80073DC4
Open: compiler thunk (this -= 0x34, tail call fn_80056ADC, the destructor); the TU of CPlayer's vtable is not recovered.
====================
*/
// 0x80073DC4
extern "C" void fn_80073DC4() {
    // TODO
}

/*
====================
fn_80073DCC
Open: compiler thunk (this -= 0x3C, tail call fn_80056ADC, the destructor); the TU of CPlayer's vtable is not recovered.
====================
*/
// 0x80073DCC
extern "C" void fn_80073DCC() {
    // TODO
}

/*
====================
fn_80073DD4
Open: compiler thunk (this -= 0x3C, tail call fn_80058774); the TU of CPlayer's vtable is not recovered.
====================
*/
// 0x80073DD4
extern "C" void fn_80073DD4() {
    // TODO
}

/*
====================
fn_80073DDC
====================
*/
// 0x80073DDC
extern "C" void fn_80073DDC() {
    // TODO
}

/*
====================
fn_80073DE4
====================
*/
// 0x80073DE4
extern "C" void fn_80073DE4() {
    // TODO
}

/*
====================
fn_80073DEC
====================
*/
// 0x80073DEC
extern "C" void fn_80073DEC() {
    // TODO
}

/*
====================
fn_80073DF4
====================
*/
// 0x80073DF4
extern "C" void fn_80073DF4() {
    // TODO
}

/*
====================
fn_80073DFC
====================
*/
// 0x80073DFC
extern "C" void fn_80073DFC() {
    // TODO
}

/*
====================
fn_80073E04
====================
*/
// 0x80073E04
extern "C" void fn_80073E04() {
    // TODO
}

/*
====================
fn_80073E0C
====================
*/
// 0x80073E0C
extern "C" void fn_80073E0C() {
    // TODO
}

/*
====================
fn_80073E14
====================
*/
// 0x80073E14
extern "C" void fn_80073E14() {
    // TODO
}

/*
====================
fn_80073E1C
====================
*/
// 0x80073E1C
extern "C" void fn_80073E1C() {
    // TODO
}

/*
====================
fn_80073E24
====================
*/
// 0x80073E24
extern "C" void fn_80073E24() {
    // TODO
}

/*
====================
fn_80073E2C
====================
*/
// 0x80073E2C
extern "C" void fn_80073E2C() {
    // TODO
}

/*
====================
fn_80073E34
====================
*/
// 0x80073E34
extern "C" void fn_80073E34() {
    // TODO
}

// Open: fn_80073554 (0x80073554) is the unit's static initialiser (__sinit); not written. Wall: referent. Its
// string literals sit up to 0x2580 bytes past lbl_803E3E70, the string pool of the whole CPlayer translation unit,
// and that unit's class code (constructor 0x800558B4, destructor 0x80056ADC, thunk target fn_800576C0) lies before
// this range in unsplit auto regions. The unit boundary is too late for the pool; report before rescoping.
