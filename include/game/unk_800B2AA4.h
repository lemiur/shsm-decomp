#ifndef GAME_UNK_800B2AA4_H
#define GAME_UNK_800B2AA4_H

#include <revolution/types.h>

/*
 * The class of this unit (constructor fn_800B2E58, vtable lbl_803EA5E8). Name invented from the
 * constructor's address: no string names it. The layout is what the constructor zeroes and what the
 * serialisers read; the names are placeholders. The vtable is not yet declared as a class (its
 * slots are free functions of this unit and of other units; see the unit's source comment).
 */
struct unk_800B2E58 {
    void*   vtbl;               // 0x00, vptr = lbl_803EA5E8
    u32     field_0x04;         // 0x04, count word of the sub-record that fn_800AC078 serialises
    u32     field_0x08;         // 0x08, halfword of that sub-record (read by fn_800AC078 at +4)
    u32     field_0x0C;         // 0x0C, serialised as 4 bytes
    u32     field_0x10;         // 0x10, serialised as 2 bytes
    u32     field_0x14;         // 0x14, serialised as 4 bytes
    u32     field_0x18;         // 0x18, count word of the second sub-record
    u32     field_0x1C;         // 0x1C, halfword of the second sub-record
    u32     field_0x20;
    u32     field_0x24;
    u32     field_0x28;
    u32     field_0x2C;
    u8      field_0x30[0x8];
    u32     field_0x38;         // 0x38, set to the RWS::Time::GetTime() value when the code is 9
    u8      field_0x3C[0x4];
    u32     field_0x40;
    u8      field_0x44;
};

extern "C" {
// Vtable of the class (the word at offset 0 of every object).
extern u8 lbl_803EA5E8[];

// Helpers of auto_03_800AB6C8_text (dtk asm, not started); signatures from their call sites.
void fn_800AC078(void* subRecord, void* stream, int flag);
void fn_800AF080(void* stream, void* value, int size);
void fn_800AF088(void* stream, void* value, int size);
void fn_800ABFE4(void* subRecord, void* stream, int flag);
void fn_800AF100(void* stream, void* value, int size);
void fn_800AF188(void* stream, void* value, int size);
unsigned int fn_800ABFE0();

void fn_800B2AA4();
void fn_800B2BC0();
void fn_800B2C04();
int  fn_800B2C08(unk_800B2E58* self, int unused, int code);
int  fn_800B2C44(unk_800B2E58* self, void* stream, int flag);
int  fn_800B2CD0(unk_800B2E58* self, void* stream, int flag);
void fn_800B2D5C();
void fn_800B2E58(unk_800B2E58* self);
unk_800B2E58* fn_800B2E94(unk_800B2E58* self, int flag);
void fn_800B2ED4();
void fn_800B2FE8();
void fn_800B3144();
void fn_800B32F4();
void fn_800B3310();
void fn_800B3434();
void fn_800B3534();
void fn_800B39D8();
void fn_800B3A20();
void fn_800B3A30();
void fn_800B3B84();
void fn_800B3CAC();
void fn_800B3D54();
void fn_800B3F64();
void fn_800B4034();
void fn_800B40A0();
void fn_800B410C();
void fn_800B4114();
}

#endif
