// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800B2AA4.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803EA5C0 size 0x28 lbl_803EA5C0
//   .data 0x803EA5E8 size 0x60 lbl_803EA5E8
//   .data 0x803EA648 size 0x18 lbl_803EA648
//   .sdata 0x805273D8 size 0x8 lbl_805273D8
//   .sdata 0x805273E0 size 0x8 lbl_805273E0
//   .sdata 0x805273E8 size 0x3 lbl_805273E8
//   .sdata 0x805273F0 size 0x8 lbl_805273F0
//   .sbss 0x80529910 size 0x8 lbl_80529910
//   .sdata2 0x8052C4B0 size 0x8 lbl_8052C4B0
//   .sdata2 0x8052C4B8 size 0x8 lbl_8052C4B8
//
// Vtable lbl_803EA5E8 (0x60 bytes): the vptr that the ctor fn_800B2E58 stores, so the slot number
// is the word index from the symbol (slot 0 is the word at lbl_803EA5E8). Slots 0 and 1 hold 0,
// as does slot 23 (no function). Slot 2 fn_800B2E94 (deleting dtor), 3 fn_800B4034 (also stores
// this vptr), 4 fn_800B40A0, 5 fn_800AD700, 6 fn_800B3534, 7 fn_800B3A30, 8 fn_800B3434,
// 9 fn_800B26F8, 10 fn_800B3A20, 11 fn_800B410C, 12 fn_800B3310, 13 fn_800B291C, 14 fn_800B2914,
// 15 fn_800B290C, 16 fn_800B3B84, 17 fn_800B3CAC, 18 fn_800B3D54, 19 fn_800B2904, 20 fn_800AE2B0,
// 21 fn_800B3F64, 22 fn_800B2C08. Slots are not yet typed: the class is unnamed.

/*
====================
fn_800B2AA4
====================
*/
// 0x800B2AA4
extern "C" void fn_800B2AA4() {
    // TODO
}

/*
====================
fn_800B2BC0
====================
*/
// 0x800B2BC0
// Open: a tail call through slot 0x58 of the fifth argument's vtable with four arguments after it; the object's
// class is not yet declared (the slot is not fn_800B2C08's signature), so the body is not written.
extern "C" void fn_800B2BC0() {
    // TODO
}

/*
====================
fn_800B2C04
====================
*/
// 0x800B2C04
extern "C" void fn_800B2C04() {
    // TODO
}

/*
====================
fn_800B2C08
====================
*/
// 0x800B2C08
extern "C" int fn_800B2C08(unk_800B2E58* self, int unused, int code) {
    if (code == 9) {
        self->field_0x38 = fn_800ABFE0();
    }
    return 0;
}

/*
====================
fn_800B2C44
====================
*/
// 0x800B2C44
extern "C" int fn_800B2C44(unk_800B2E58* self, void* stream, int flag) {
    fn_800AC078(&self->field_0x04, stream, flag);
    fn_800AF080(stream, &self->field_0x0C, 4);
    fn_800AF088(stream, &self->field_0x10, 2);
    fn_800AF080(stream, &self->field_0x14, 4);
    fn_800AC078(&self->field_0x18, stream, flag);
    return 0;
}

/*
====================
fn_800B2CD0
====================
*/
// 0x800B2CD0
extern "C" int fn_800B2CD0(unk_800B2E58* self, void* stream, int flag) {
    fn_800ABFE4(&self->field_0x04, stream, flag);
    fn_800AF100(stream, &self->field_0x0C, 4);
    fn_800AF188(stream, &self->field_0x10, 2);
    fn_800AF100(stream, &self->field_0x14, 4);
    fn_800ABFE4(&self->field_0x18, stream, flag);
    return 0;
}

/*
====================
fn_800B2D5C
====================
*/
// 0x800B2D5C
// Open: a copy of two u16-counted arrays plus three scalars. Its halfword store to +0x8 conflicts with the
// constructor's word store to +0x8, so the field type of +0x8 is not settled.
extern "C" void fn_800B2D5C() {
    // TODO
}

/*
====================
fn_800B2E58
====================
*/
// 0x800B2E58
extern "C" void fn_800B2E58(unk_800B2E58* self) {
    self->vtbl = lbl_803EA5E8;
    self->field_0x04 = 0;
    self->field_0x08 = 0;
    self->field_0x0C = 0;
    self->field_0x2C = 0;
    self->field_0x10 = 0;
    self->field_0x20 = 0;
    self->field_0x24 = 0;
    self->field_0x40 = 0;
    self->field_0x28 = 0;
    self->field_0x44 = 0;
}

/*
====================
fn_800B2E94
====================
*/
// 0x800B2E94
extern "C" unk_800B2E58* fn_800B2E94(unk_800B2E58* self, int flag) {
    if (self != NULL) {
        if (flag > 0) {
            operator delete(self);
        }
    }
    return self;
}

/*
====================
fn_800B2ED4
====================
*/
// 0x800B2ED4
extern "C" void fn_800B2ED4() {
    // TODO
}

/*
====================
fn_800B2FE8
====================
*/
// 0x800B2FE8
extern "C" void fn_800B2FE8() {
    // TODO
}

/*
====================
fn_800B3144
====================
*/
// 0x800B3144
extern "C" void fn_800B3144() {
    // TODO
}

/*
====================
fn_800B32F4
====================
*/
// 0x800B32F4
extern "C" void fn_800B32F4() {
    // TODO
}

/*
====================
fn_800B3310
====================
*/
// 0x800B3310
extern "C" void fn_800B3310() {
    // TODO
}

/*
====================
fn_800B3434
====================
*/
// 0x800B3434
extern "C" void fn_800B3434() {
    // TODO
}

/*
====================
fn_800B3534
====================
*/
// 0x800B3534
extern "C" void fn_800B3534() {
    // TODO
}

/*
====================
HUD::cFifo<8,Q23HUD13sNotification>::Clear
====================
*/
// 0x800B39C4
// TODO: signature not expanded or needs a complete type: Clear__Q23HUD30cFifo<8,Q23HUD13sNotification>Fv
// void HUD::cFifo<8,Q23HUD13sNotification>::Clear() { }

/*
====================
fn_800B39D8
====================
*/
// 0x800B39D8
extern "C" void fn_800B39D8() {
    // TODO
}

/*
====================
fn_800B3A20
====================
*/
// 0x800B3A20
extern "C" void fn_800B3A20() {
    // TODO
}

/*
====================
fn_800B3A30
====================
*/
// 0x800B3A30
extern "C" void fn_800B3A30() {
    // TODO
}

/*
====================
fn_800B3B84
====================
*/
// 0x800B3B84
extern "C" void fn_800B3B84() {
    // TODO
}

/*
====================
fn_800B3CAC
====================
*/
// 0x800B3CAC
extern "C" void fn_800B3CAC() {
    // TODO
}

/*
====================
fn_800B3D54
====================
*/
// 0x800B3D54
extern "C" void fn_800B3D54() {
    // TODO
}

/*
====================
fn_800B3F64
====================
*/
// 0x800B3F64
extern "C" void fn_800B3F64() {
    // TODO
}

/*
====================
fn_800B4034
====================
*/
// 0x800B4034
extern "C" void fn_800B4034() {
    // TODO
}

/*
====================
fn_800B40A0
====================
*/
// 0x800B40A0
extern "C" void fn_800B40A0() {
    // TODO
}

/*
====================
fn_800B410C
====================
*/
// 0x800B410C
extern "C" void fn_800B410C() {
    // TODO
}

/*
====================
fn_800B4114
====================
*/
// 0x800B4114
extern "C" void fn_800B4114() {
    // TODO
}
