// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8001B200.h>

#include <climax/Utils.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E0A70 size 0x1F lbl_803E0A70
//   .data 0x803E0A90 size 0x1F lbl_803E0A90
//   .data 0x803E0AB0 size 0x39 lbl_803E0AB0
//   .data 0x803E0AF0 size 0x50 lbl_803E0AF0
//   .data 0x803E0B40 size 0x60 lbl_803E0B40
//   .sdata 0x805269A8 size 0x4 lbl_805269A8
//   .sdata 0x805269AC size 0x4 lbl_805269AC
//   .sdata 0x805269B0 size 0x4 lbl_805269B0
//   .sdata 0x805269B4 size 0x4 lbl_805269B4
//   .sdata 0x805269B8 size 0x4 lbl_805269B8
//   .sdata 0x805269BC size 0x4 lbl_805269BC
//   .sdata 0x805269C0 size 0x4 lbl_805269C0
//   .sdata 0x805269C4 size 0x4 lbl_805269C4
//   .sdata2 0x8052B7B8 size 0x4 lbl_8052B7B8
//   .sdata2 0x8052B7BC size 0x4 lbl_8052B7BC
//   .sdata2 0x8052B7C0 size 0x4 lbl_8052B7C0
//   .sdata2 0x8052B7C4 size 0x4 lbl_8052B7C4
//   .sdata2 0x8052B7C8 size 0x4 lbl_8052B7C8
//   .sdata2 0x8052B7CC size 0x4 lbl_8052B7CC
//   .sdata2 0x8052B7D0 size 0x4 lbl_8052B7D0
//   .sdata2 0x8052B7D4 size 0x4 lbl_8052B7D4
//   .sdata2 0x8052B7D8 size 0x4 lbl_8052B7D8
//   .sdata2 0x8052B7E0 size 0x8 lbl_8052B7E0
//   .sdata2 0x8052B7E8 size 0x4 lbl_8052B7E8
//   .sdata2 0x8052B7EC size 0x4 lbl_8052B7EC
//   .sdata2 0x8052B7F0 size 0x4 lbl_8052B7F0
//   .sdata2 0x8052B7F4 size 0x4 lbl_8052B7F4
//   .sdata2 0x8052B7F8 size 0x4 lbl_8052B7F8
//   .sdata2 0x8052B7FC size 0x4 lbl_8052B7FC
//   .sdata2 0x8052B800 size 0x4 lbl_8052B800
//   .sdata2 0x8052B804 size 0x4 lbl_8052B804
//   .sdata2 0x8052B808 size 0x4 lbl_8052B808
//   .sdata2 0x8052B80C size 0x4 lbl_8052B80C

/*
====================
fn_8001B200
====================
*/
// 0x8001B200
unk_RwV3d fn_8001B200( unk_8001B000* obj ) {
    // TODO: retail reads obj->field_0x8 and calls its vtable slot 0x30 (fn_8001B27C) for the vector,
    // or yields zero. Needs the controller class: its vtable is lbl_803E0B40 (22 virtual slots from +0x8).
}

/*
====================
fn_8001B27C
====================
*/
// 0x8001B27C
// Returns the vector field_0x24 by value (the vtable slot +0x30 target; sret in r3, obj in r4).
// Wall (schedule): retail copies one component at a time (lfs/stfs pairs through f0), the compiler
// hoists the loads above the stores. Not matched.
extern "C" unk_RwV3d fn_8001B27C( const unk_8001B000* obj ) {
    return obj->field_0x24;
}

/*
====================
fn_8001B298
====================
*/
// 0x8001B298
extern "C" float fn_8001B298( const unk_8001B000* obj ) {
    return obj->field_0x3C;
}

/*
====================
fn_8001B2A0
====================
*/
// Finds the entry called name (compared without case) in the list at field_0x30, or NULL.
// 0x8001B2A0
extern "C" unk_8001B0Entry* fn_8001B2A0( const unk_8001B000* obj, const char* name ) {
    u32 hash = RWS::SHUtils::GetStringHashForceLower( name, -1 );
    for ( unk_8001B0Node* node = obj->field_0x30; node != NULL; node = node->next ) {
        if ( hash == node->data->hash && fn_801B6138( name, node->data->name ) ) {
            return node->data;
        }
    }
    return NULL;
}

/*
====================
fn_8001B330
====================
*/
// Stub. Wall (class): virtual calls on this and on a second object's function-pointer fields, plus
// fn_800A2A18 and fn_800A2AB8; needs the controller class and those helpers.
// 0x8001B330
extern "C" void fn_8001B330() {
    // TODO
}

/*
====================
fn_8001B5DC
====================
*/
// Stub. Wall (class): calls virtual slot +0x44 (fn_8001D27C) on this before storing field_0x18;
// needs the controller class.
// 0x8001B5DC
extern "C" void fn_8001B5DC() {
    // TODO
}

/*
====================
fn_8001B650
====================
*/
// Stub. Wall (class): stores field_0x14, then tail-calls virtual slot +0x40 (fn_8001D158) on this;
// needs the controller class.
// 0x8001B650
extern "C" void fn_8001B650() {
    // TODO
}

/*
====================
fn_8001B664
====================
*/
// Stub. Wall (helper): the callee fn_800A2A18 has no recovered prototype, and the record at
// field_0x50 (argument words, cached key and result) is not yet recovered.
// 0x8001B664
extern "C" void fn_8001B664() {
    // TODO
}

/*
====================
fn_8001B6F8
====================
*/
// Stub. Wall (class): calls virtual slot +0x3C on each entry of the list at field_0x20; the entry
// class is not yet recovered.
// 0x8001B6F8
extern "C" void fn_8001B6F8() {
    // TODO
}

/*
====================
fn_8001B764
====================
*/
// 0x8001B764
// Retail is a bare blr: an empty function (no body to recover).
extern "C" void fn_8001B764() {
}

/*
====================
fn_8001B768
====================
*/
// 0x8001B768
extern "C" void fn_8001B768( unk_8001B000* obj ) {
    obj->field_0x1C = NULL;
    obj->field_0x20 = NULL;
}

/*
====================
fn_8001B778
====================
*/
// 0x8001B778
// Setter for the three pointer slots, keyed by id (name invented); a free function until the
// controller class is recovered (see the header).
extern "C" void fn_8001B778( unk_8001B000* obj, s32 id, void* value ) {
    switch ( id ) {
    case 8:
        obj->field_0x48 = value;
        break;
    case 0xA:
        obj->field_0x4C = value;
        break;
    case 0xB:
        obj->field_0x50 = value;
        break;
    }
}

/*
====================
fn_8001B7AC
====================
*/
// 0x8001B7AC
extern "C" void fn_8001B7AC() {
    // TODO
}

/*
====================
fn_8001B884
====================
*/
// 0x8001B884
extern "C" void fn_8001B884() {
    // TODO
}

/*
====================
fn_8001BA3C
====================
*/
// 0x8001BA3C
extern "C" void fn_8001BA3C() {
    // TODO
}

/*
====================
fn_8001C42C
====================
*/
// 0x8001C42C
extern "C" void fn_8001C42C() {
    // TODO
}

/*
====================
fn_8001C5F8
====================
*/
// 0x8001C5F8
extern "C" void fn_8001C5F8() {
    // TODO
}

/*
====================
fn_8001C7B8
====================
*/
// 0x8001C7B8
extern "C" void fn_8001C7B8() {
    // TODO
}

/*
====================
fn_8001C9B8
====================
*/
// 0x8001C9B8
extern "C" void fn_8001C9B8() {
    // TODO
}

/*
====================
fn_8001CB2C
====================
*/
// 0x8001CB2C
extern "C" void fn_8001CB2C() {
    // TODO
}

/*
====================
fn_8001CC1C
====================
*/
// 0x8001CC1C
extern "C" void fn_8001CC1C() {
    // TODO
}

/*
====================
fn_8001CD1C
====================
*/
// 0x8001CD1C
extern "C" void fn_8001CD1C() {
    // TODO
}

/*
====================
fn_8001CD24
====================
*/
// 0x8001CD24
extern "C" void fn_8001CD24() {
    // TODO
}

/*
====================
fn_8001CD30
====================
*/
// 0x8001CD30
extern "C" void fn_8001CD30() {
    // TODO
}

/*
====================
fn_8001CF74
====================
*/
// 0x8001CF74
extern "C" void fn_8001CF74() {
    // TODO
}

/*
====================
fn_8001CF78
====================
*/
// 0x8001CF78
extern "C" void fn_8001CF78() {
    // TODO
}

/*
====================
fn_8001D114
====================
*/
// 0x8001D114
extern "C" void fn_8001D114() {
    // TODO
}

/*
====================
fn_8001D158
====================
*/
// 0x8001D158
extern "C" void fn_8001D158() {
    // TODO
}

/*
====================
fn_8001D254
====================
*/
// 0x8001D254
extern "C" void fn_8001D254() {
    // TODO
}

/*
====================
fn_8001D27C
====================
*/
// 0x8001D27C
extern "C" void fn_8001D27C() {
    // TODO
}

/*
====================
fn_8001D454
====================
*/
// 0x8001D454
extern "C" void fn_8001D454() {
    // TODO
}

/*
====================
fn_8001D580
====================
*/
// 0x8001D580
extern "C" void fn_8001D580() {
    // TODO
}

/*
====================
fn_8001D6C0
====================
*/
// 0x8001D6C0
extern "C" void fn_8001D6C0() {
    // TODO
}

/*
====================
fn_8001D934
====================
*/
// 0x8001D934
extern "C" void fn_8001D934() {
    // TODO
}

/*
====================
fn_8001DB7C
====================
*/
// 0x8001DB7C
extern "C" void fn_8001DB7C() {
    // TODO
}

/*
====================
fn_8001DF20
====================
*/
// 0x8001DF20
extern "C" void fn_8001DF20() {
    // TODO
}

/*
====================
fn_8001DF70
====================
*/
// 0x8001DF70
extern "C" void fn_8001DF70() {
    // TODO
}

/*
====================
fn_8001E390
====================
*/
// 0x8001E390
extern "C" void fn_8001E390() {
    // TODO
}

/*
====================
fn_8001E5A8
====================
*/
// 0x8001E5A8
extern "C" void fn_8001E5A8() {
    // TODO
}

/*
====================
fn_8001E5FC
====================
*/
// 0x8001E5FC
extern "C" void fn_8001E5FC() {
    // TODO
}

/*
====================
fn_8001E820
====================
*/
// 0x8001E820
extern "C" void fn_8001E820() {
    // TODO
}

/*
====================
fn_8001ECC0
====================
*/
// 0x8001ECC0
extern "C" void fn_8001ECC0() {
    // TODO
}

/*
====================
fn_8001EEB0
====================
*/
// 0x8001EEB0
extern "C" void fn_8001EEB0() {
    // TODO
}

/*
====================
fn_8001EF3C
====================
*/
// 0x8001EF3C
extern "C" void fn_8001EF3C() {
    // TODO
}

/*
====================
fn_8001EF44
====================
*/
// 0x8001EF44
extern "C" void fn_8001EF44() {
    // TODO
}

/*
====================
fn_8001EF70
====================
*/
// 0x8001EF70
extern "C" void fn_8001EF70() {
    // TODO
}

/*
====================
fn_8001EFC4
====================
*/
// 0x8001EFC4
extern "C" void fn_8001EFC4() {
    // TODO
}

/*
====================
fn_8001F12C
====================
*/
// 0x8001F12C
extern "C" void fn_8001F12C() {
    // TODO
}

/*
====================
fn_8001F170
====================
*/
// 0x8001F170
extern "C" void fn_8001F170() {
    // TODO
}

/*
====================
fn_8001F260
====================
*/
// 0x8001F260
extern "C" void fn_8001F260() {
    // TODO
}

/*
====================
fn_8001F48C
====================
*/
// 0x8001F48C
extern "C" void fn_8001F48C() {
    // TODO
}

/*
====================
fn_8001F4E4
====================
*/
// 0x8001F4E4
extern "C" void fn_8001F4E4() {
    // TODO
}

/*
====================
fn_8001F63C
====================
*/
// 0x8001F63C
extern "C" void fn_8001F63C() {
    // TODO
}

/*
====================
fn_8001F708
====================
*/
// 0x8001F708
extern "C" void fn_8001F708() {
    // TODO
}

/*
====================
fn_8001F9C8
====================
*/
// 0x8001F9C8
extern "C" void fn_8001F9C8() {
    // TODO
}

/*
====================
fn_8001FAF4
====================
*/
// 0x8001FAF4
extern "C" void fn_8001FAF4() {
    // TODO
}

/*
====================
fn_8001FAF8
====================
*/
// 0x8001FAF8
extern "C" void fn_8001FAF8() {
    // TODO
}

/*
====================
fn_8001FB00
====================
*/
// 0x8001FB00
extern "C" void fn_8001FB00() {
    // TODO
}

/*
====================
fn_8001FB08
====================
*/
// 0x8001FB08
extern "C" void fn_8001FB08() {
    // TODO
}

/*
====================
fn_8001FB0C
====================
*/
// 0x8001FB0C
extern "C" void fn_8001FB0C() {
    // TODO
}

/*
====================
fn_8001FB30
====================
*/
// 0x8001FB30
extern "C" void fn_8001FB30() {
    // TODO
}

/*
====================
fn_8001FBAC
====================
*/
// 0x8001FBAC
extern "C" void fn_8001FBAC() {
    // TODO
}

/*
====================
fn_8001FBCC
====================
*/
// 0x8001FBCC
extern "C" void fn_8001FBCC() {
    // TODO
}

/*
====================
fn_8001FBEC
====================
*/
// 0x8001FBEC
extern "C" void fn_8001FBEC() {
    // TODO
}

/*
====================
fn_8001FC10
====================
*/
// 0x8001FC10
extern "C" void fn_8001FC10() {
    // TODO
}

/*
====================
fn_8001FC18
====================
*/
// 0x8001FC18
extern "C" void fn_8001FC18() {
    // TODO
}

/*
====================
fn_8001FC20
====================
*/
// 0x8001FC20
extern "C" void fn_8001FC20() {
    // TODO
}

/*
====================
fn_8001FC94
====================
*/
// 0x8001FC94
extern "C" void fn_8001FC94() {
    // TODO
}

/*
====================
fn_8001FD18
====================
*/
// 0x8001FD18
extern "C" void fn_8001FD18() {
    // TODO
}

/*
====================
fn_8001FDA0
====================
*/
// 0x8001FDA0
extern "C" void fn_8001FDA0() {
    // TODO
}

/*
====================
fn_8001FDA8
====================
*/
// 0x8001FDA8
extern "C" void fn_8001FDA8() {
    // TODO
}

/*
====================
fn_8001FE34
====================
*/
// 0x8001FE34
extern "C" void fn_8001FE34() {
    // TODO
}

/*
====================
fn_8001FE3C
====================
*/
// 0x8001FE3C
extern "C" void fn_8001FE3C() {
    // TODO
}

/*
====================
fn_80020000
====================
*/
// 0x80020000
extern "C" void fn_80020000() {
    // TODO
}

/*
====================
fn_80020024
====================
*/
// 0x80020024
extern "C" void fn_80020024() {
    // TODO
}

/*
====================
fn_800200AC
====================
*/
// 0x800200AC
extern "C" void fn_800200AC() {
    // TODO
}

/*
====================
fn_80020120
====================
*/
// 0x80020120
extern "C" void fn_80020120() {
    // TODO
}

/*
====================
fn_800201A8
====================
*/
// 0x800201A8
extern "C" void fn_800201A8() {
    // TODO
}

/*
====================
fn_800201B0
====================
*/
// 0x800201B0
extern "C" void fn_800201B0() {
    // TODO
}

/*
====================
fn_800201B8
====================
*/
// 0x800201B8
extern "C" void fn_800201B8() {
    // TODO
}
