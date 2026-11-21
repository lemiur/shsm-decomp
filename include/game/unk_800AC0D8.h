#ifndef GAME_UNK_800AC0D8_H
#define GAME_UNK_800AC0D8_H

#include <revolution/types.h>

// Class names in this header are invented: each is named after its constructor address.

// Element id before the element is loaded (stored as a u16).
enum { UNK_ID_UNSET = 0xFFFF };

// Object owned by unk_800AC518 at +0x8. Only the byte read by fn_800ACAC4 is named.
struct unk_800AC518Owner {
    u8 field_0x0[0xB4];
    u8 m_flagB4;
};

// Element of the 0x30-byte array in unk_800AC24C (constructor fn_800AC0D8, destructor fn_800AC128).
struct unk_800AC0D8 {
    u16 m_id;               // 0x00, 0xFFFF until loaded
    u16* m_pIndices;        // 0x04, array of m_count u16 values
    u16 m_count;            // 0x08
    void* m_pObj0C;         // 0x0C, polymorphic object (deleted through its vtable)
    u16 m_flags;            // 0x10
    float m_f14;            // 0x14
    float m_f18;            // 0x18, -1.0f
    void* m_pObj1C;         // 0x1C
    void* m_pObj20;         // 0x20, polymorphic object (virtual slot 19 is called)
    float m_f24;            // 0x24
    s32 m_n28;              // 0x28, 5
    u8 m_b2C;               // 0x2C
    u8 m_b2D;               // 0x2D
};

// Sorted entry of unk_800AC518 (12 bytes).
struct unk_800AC518Entry {
    float m_key;
    void* m_a;
    void* m_b;
};

// List class embedded at +0x728 of unk_800AC518. Its constructor is inline in fn_800AC518;
// its destructor is fn_800AC5BC. Node links: +0x4 prev, +0x8 next.
struct unk_800AC5BC {
    void* __vptr;           // 0x00
    s32 m_count;            // 0x04
    void* m_pLast;          // 0x08
    void* m_pFirst;         // 0x0C
};

// Node of unk_800AC5BC.
struct unk_800AC5BCNode {
    u8 field_0x0[4];
    unk_800AC5BCNode* m_pPrev;  // 0x04
    unk_800AC5BCNode* m_pNext;  // 0x08
};

// Set loaded by fn_800AC24C; unk_800AC518 holds one at +0x0.
struct unk_800AC24C {
    u8 field_0x00[8];
    u32 m_n8;               // 0x08
    u16 m_count0C;          // 0x0C
    void* m_pRecords;       // 0x10, 16-byte records: u16 index, u32, u32
    u16 m_count18;          // 0x18
    unk_800AC0D8* m_pElems; // 0x1C, 0x30-byte elements
};

struct unk_800AC518 {
    unk_800AC24C* m_pSet;                 // 0x000
    u32 field_0x004;                 // 0x004
    unk_800AC518Owner* m_pOwner;             // 0x008
    u8 field_0x00C[0x40];            // 0x00C
    u8 m_b4C;                        // 0x04C
    u8 field_0x04D[0x7F];            // 0x04D
    u8 m_bCC;                        // 0x0CC
    u8 field_0x0CD[7];               // 0x0CD
    u8 m_bD4;                        // 0x0D4
    u8 field_0x0D5[3];               // 0x0D5
    unk_800AC518Entry m_entries[127];      // 0x0D8, sorted by m_key
    u8 field_0x6CC[0xC];             // 0x6CC
    u32 m_entryCount;                // 0x6D8
    float m_f6DC;                    // 0x6DC, 1.0f
    float m_f6E0;                    // 0x6E0, 0.0f
    u32 field_0x6E4;                 // 0x6E4
    u32 field_0x6E8;                 // 0x6E8
    u32 field_0x6EC;                 // 0x6EC
    u32 field_0x6F0;                 // 0x6F0
    u32 field_0x6F4;                 // 0x6F4
    u32 field_0x6F8;                 // 0x6F8
    u8 field_0x6FC[8];               // 0x6FC
    u8 m_b704;                       // 0x704
    u8 field_0x705[3];               // 0x705
    u32 field_0x708;                 // 0x708
    u8 field_0x70C[8];               // 0x70C
    u32 field_0x714;                 // 0x714
    float m_f718;                    // 0x718, 0.0f
    u32 field_0x71C;                 // 0x71C
    u32 m_u720;                      // 0x720, 1500
    u32 m_u724;                      // 0x724
    unk_800AC5BC m_list;                 // 0x728
};

extern "C" {
extern void* lbl_803EA0E8[4];  // vtable of unk_800AC5BC: offset 0x8 is fn_800AC5BC
void fn_800AC0D8(unk_800AC0D8* self);
void fn_800AC128(unk_800AC0D8* self, s32 flags);
void fn_800AC24C();
void fn_800AC518(unk_800AC518* self);
unk_800AC5BC* fn_800AC5BC(unk_800AC5BC* self, s32 flags);
void fn_800AC694();
void fn_800AC7AC();
void fn_800AC944();
u8 fn_800ACAC4(unk_800AC518* self);
s32 fn_800ACAE0(unk_800AC518* self, float key, void* a, void* b);
void fn_800ACB94();
void fn_800ACC2C();
void fn_800ACC30();
void fn_800ACD0C();
void fn_800ACF18();
void fn_800AD14C();
void fn_800AD2BC();
void fn_800AD3DC();
void fn_800AD42C();
void fn_800AD558();
void fn_800AD670();
void fn_800AD6F8();
void fn_800AD700();
void fn_800AD708();
void fn_800AD710();
void fn_800AD80C();
void fn_800AE244();
void fn_800AE24C();
void fn_800AE258();
void fn_800AE2A8();
void fn_800AE2B0();
void fn_800AE2B8();
void fn_800AE2D8();
void fn_800AE3A8();
void fn_800AE498();
void fn_800AE4A4();
void fn_800AE550();
void fn_800AE558();
void fn_800AE610();
void fn_800AE6F8();
void fn_800AEF30();
}

#endif
