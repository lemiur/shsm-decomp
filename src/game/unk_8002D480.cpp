// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8002D480.h>

// The 0xA4-byte object built by fn_8002D480 (its constructor fn_8002D544) has four vtables, one per
// sub-object, as the constructor stores them: lbl_803E1C80 at offset 0, lbl_803E1CA8 (__vt__12unk_NodeList)
// at 0x8, lbl_803E1C9C at 0x1C and lbl_803E1C90 at 0x30. fn_8002D7C4 is the destructor of unk_NodeList
// (vtable lbl_803E1CA8) and fn_8002D89C the destructor of unk_803DFC38 (vtable lbl_803E1C9C): each listing
// is the same as its exact counterpart in src/game/unk_8000EC94.cpp up to relocation names and branch targets.
// fn_8002D974 is not a copy of either (0xB4 bytes, not 0xD8).

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E1998 size 0x9 lbl_803E1998
//   .data 0x803E19A4 size 0xE lbl_803E19A4
//   .data 0x803E19B4 size 0xE lbl_803E19B4
//   .data 0x803E19C4 size 0x17 lbl_803E19C4
//   .data 0x803E19DC size 0x17 lbl_803E19DC
//   .data 0x803E19F4 size 0x17 lbl_803E19F4
//   .data 0x803E1A0C size 0x15 lbl_803E1A0C
//   .data 0x803E1A24 size 0x19 lbl_803E1A24
//   .data 0x803E1A40 size 0x19 lbl_803E1A40
//   .data 0x803E1A5C size 0x19 lbl_803E1A5C
//   .data 0x803E1A78 size 0x14 lbl_803E1A78
//   .data 0x803E1A90 size 0x98 lbl_803E1A90
//   .data 0x803E1B28 size 0x10 lbl_803E1B28
//   .data 0x803E1B38 size 0x148 lbl_803E1B38
//   .data 0x803E1C80 size 0x10 lbl_803E1C80
//   .data 0x803E1C90 size 0xC lbl_803E1C90
//   .data 0x803E1C9C size 0xC lbl_803E1C9C
//   .data 0x803E1CA8 size 0x10 lbl_803E1CA8
//   .sdata 0x80526A28 size 0x4 lbl_80526A28
//   .sdata 0x80526A2C size 0x4 lbl_80526A2C
//   .sdata 0x80526A30 size 0x4 lbl_80526A30
//   .sdata 0x80526A34 size 0x4 lbl_80526A34
//   .sdata 0x80526A38 size 0x4 lbl_80526A38
//   .sdata 0x80526A3C size 0x4 lbl_80526A3C
//   .sdata 0x80526A40 size 0x4 lbl_80526A40
//   .sdata 0x80526A44 size 0x4 lbl_80526A44
//   .sdata2 0x8052B8D0 size 0x4 lbl_8052B8D0
//   .sdata2 0x8052B8D4 size 0x4 lbl_8052B8D4
//   .sdata2 0x8052B8D8 size 0x4 lbl_8052B8D8

/*
====================
fn_8002D480
====================
*/
// 0x8002D480
extern "C" void fn_8002D480() {
    unk_RespotManager* manager = ( unk_RespotManager* )operator new( 0xA4, GAME_HEAP, GAME_HEAP_FLAGS );
    if ( manager != NULL ) {
        manager = fn_8002D544( manager );
    }
    lbl_80529260 = manager;
}

/*
====================
fn_8002D4BC
====================
*/
// 0x8002D4BC
extern "C" void fn_8002D4BC() {
    // TODO
}

/*
====================
fn_8002D544
====================
*/
// 0x8002D544
// Stub until the constructor is reconstructed: dont_inline keeps the call the factory makes in retail.
#pragma dont_inline on
extern "C" unk_RespotManager* fn_8002D544( unk_RespotManager* self ) {
    // TODO
    return self;
}
#pragma dont_inline off

/*
====================
fn_8002D7C4
====================
*/
// 0x8002D7C4
extern "C" void fn_8002D7C4() {
    // TODO
}

/*
====================
fn_8002D89C
====================
*/
// 0x8002D89C
extern "C" void fn_8002D89C() {
    // TODO
}

/*
====================
fn_8002D974
====================
*/
// 0x8002D974
extern "C" void fn_8002D974() {
    // TODO
}

/*
====================
fn_8002DA28
====================
*/
// 0x8002DA28
extern "C" void fn_8002DA28() {
    // TODO
}

/*
====================
fn_8002DD18
====================
*/
// 0x8002DD18
extern "C" void fn_8002DD18() {
    // TODO
}

/*
====================
fn_8002DF34
====================
*/
// 0x8002DF34
extern "C" void fn_8002DF34() {
    // TODO
}

/*
====================
fn_8002DFBC
====================
*/
// 0x8002DFBC
extern "C" void fn_8002DFBC() {
    // TODO
}

/*
====================
fn_8002E004
====================
*/
// 0x8002E004
extern "C" void fn_8002E004() {
    // TODO
}

/*
====================
fn_8002E17C
====================
*/
// 0x8002E17C
extern "C" void fn_8002E17C() {
    // TODO
}

/*
====================
unk_80055174::fn_800C4E8C
====================
*/
// 0x8002E208
CZone* unk_80055174::fn_800C4E8C() {
    // TODO
    return 0;
}

/*
====================
fn_8002E210
====================
*/
// 0x8002E210
extern "C" void fn_8002E210() {
    // TODO
}

/*
====================
fn_8002E2C4
====================
*/
// 0x8002E2C4
extern "C" void fn_8002E2C4() {
    // TODO
}

/*
====================
fn_8002E314
====================
*/
// 0x8002E314
extern "C" void fn_8002E314() {
    // TODO
}

/*
====================
fn_8002E364
====================
*/
// 0x8002E364
extern "C" void fn_8002E364() {
    // TODO
}

/*
====================
fn_8002EA80
====================
*/
// 0x8002EA80
extern "C" void fn_8002EA80() {
    // TODO
}

/*
====================
fn_8002ECE0
====================
*/
// 0x8002ECE0
extern "C" void fn_8002ECE0() {
    // TODO
}

/*
====================
fn_8002EEE8
====================
*/
// 0x8002EEE8
extern "C" void fn_8002EEE8() {
    // TODO
}

/*
====================
fn_8002F4C8
====================
*/
// 0x8002F4C8
extern "C" void fn_8002F4C8() {
    // TODO
}

/*
====================
fn_8002F500
====================
*/
// 0x8002F500
extern "C" void fn_8002F500() {
    // TODO
}

/*
====================
fn_8002F538
====================
*/
// 0x8002F538
extern "C" void fn_8002F538() {
    // TODO
}

/*
====================
unk_RespotManager::fn_8002F808
====================
*/
// 0x8002F808
void unk_RespotManager::fn_8002F808( const char* ) {
    // TODO
}

/*
====================
fn_8002FAD8
====================
*/
// 0x8002FAD8
extern "C" void fn_8002FAD8() {
    // TODO
}

/*
====================
fn_8002FC8C
====================
*/
// 0x8002FC8C
extern "C" void fn_8002FC8C() {
    // TODO
}

/*
====================
fn_8002FDB8
====================
*/
// 0x8002FDB8
extern "C" void fn_8002FDB8() {
    // TODO
}

/*
====================
fn_8002FF2C
====================
*/
// 0x8002FF2C
extern "C" void fn_8002FF2C() {
    // TODO
}

/*
====================
fn_800300B0
====================
*/
// 0x800300B0
extern "C" void fn_800300B0() {
    // TODO
}

/*
====================
fn_80030158
====================
*/
// 0x80030158
extern "C" void fn_80030158() {
    // TODO
}

/*
====================
fn_800301EC
====================
*/
// 0x800301EC
extern "C" void fn_800301EC() {
    // TODO
}

/*
====================
unk_RespotManager::fn_80030254
====================
*/
// 0x80030254
u32 unk_RespotManager::fn_80030254() {
    // TODO
    return 0;
}

/*
====================
fn_80030284
====================
*/
// 0x80030284
extern "C" void fn_80030284() {
    // TODO
}

/*
====================
fn_8003030C
====================
*/
// 0x8003030C
extern "C" void fn_8003030C() {
    // TODO
}

/*
====================
unk_RespotManager::fn_80030314
====================
*/
// 0x80030314
const char* unk_RespotManager::fn_80030314( u32, const char* ) {
    // TODO
    return 0;
}

/*
====================
fn_800303F0
====================
*/
// 0x800303F0
extern "C" void fn_800303F0() {
    // TODO
}

/*
====================
fn_80030638
====================
*/
// 0x80030638
extern "C" void fn_80030638() {
    // TODO
}

/*
====================
fn_800306F8
====================
*/
// 0x800306F8
extern "C" void fn_800306F8() {
    // TODO
}

/*
====================
fn_80030750
====================
*/
// 0x80030750
extern "C" void fn_80030750() {
    // TODO
}

/*
====================
fn_800308E8
====================
*/
// 0x800308E8
extern "C" void fn_800308E8() {
    // TODO
}

/*
====================
fn_800309B4
====================
*/
// 0x800309B4
extern "C" void fn_800309B4() {
    // TODO
}

/*
====================
fn_80030A28
====================
*/
// 0x80030A28
extern "C" void fn_80030A28() {
    // TODO
}

/*
====================
fn_80030A60
====================
*/
// 0x80030A60
extern "C" void fn_80030A60() {
    // TODO
}

/*
====================
fn_80030B68
====================
*/
// 0x80030B68
extern "C" void fn_80030B68() {
    // TODO
}

/*
====================
fn_80030BEC
====================
*/
// 0x80030BEC
extern "C" void fn_80030BEC() {
    // TODO
}

/*
====================
fn_80030D58
====================
*/
// 0x80030D58
extern "C" void fn_80030D58() {
    // TODO
}

/*
====================
fn_80030ED4
====================
*/
// 0x80030ED4
extern "C" void fn_80030ED4() {
    // TODO
}

/*
====================
fn_80030F00
====================
*/
// 0x80030F00
extern "C" void fn_80030F00() {
    // TODO
}

/*
====================
fn_80030F20
====================
*/
// 0x80030F20
extern "C" void fn_80030F20() {
    // TODO
}

/*
====================
unk_RespotManager::fn_80030F34
====================
*/
// 0x80030F34
void unk_RespotManager::fn_80030F34( const char*, const char*, const char* ) {
    // TODO
}

/*
====================
fn_80031020
====================
*/
// 0x80031020
extern "C" void fn_80031020() {
    // TODO
}

/*
====================
fn_80031100
====================
*/
// 0x80031100
extern "C" void fn_80031100() {
    // TODO
}

/*
====================
fn_80031320
====================
*/
// 0x80031320
extern "C" void fn_80031320() {
    // TODO
}

/*
====================
fn_80031568
====================
*/
// 0x80031568
extern "C" void fn_80031568() {
    // TODO
}

/*
====================
fn_800315AC
====================
*/
// 0x800315AC
extern "C" void fn_800315AC() {
    // TODO
}

/*
====================
fn_800315B8
====================
*/
// 0x800315B8
extern "C" void fn_800315B8() {
    // TODO
}

/*
====================
fn_80031760
====================
*/
// 0x80031760
extern "C" void fn_80031760() {
    // TODO
}

/*
====================
fn_800317B8
====================
*/
// 0x800317B8
extern "C" void fn_800317B8() {
    // TODO
}

/*
====================
fn_80031808
====================
*/
// 0x80031808
extern "C" void fn_80031808() {
    // TODO
}

/*
====================
fn_800318C4
====================
*/
// 0x800318C4
extern "C" void fn_800318C4() {
    // TODO
}

/*
====================
fn_80031980
====================
*/
// 0x80031980
extern "C" void fn_80031980() {
    // TODO
}

/*
====================
fn_80031CE0
====================
*/
// 0x80031CE0
extern "C" void fn_80031CE0() {
    // TODO
}

/*
====================
fn_80031E18
====================
*/
// 0x80031E18
extern "C" void fn_80031E18() {
    // TODO
}

/*
====================
fn_80031E6C
====================
*/
// 0x80031E6C
extern "C" void fn_80031E6C() {
    // TODO
}
