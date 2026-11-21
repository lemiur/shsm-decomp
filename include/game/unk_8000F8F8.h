#ifndef GAME_UNK_8000F8F8_H
#define GAME_UNK_8000F8F8_H

#include <revolution/types.h>

/* One named row of a unk_Table (16 bytes). Name invented. */
struct unk_TableRow {
    const char* key;        // 0x0, the row's name, searched by fn_80188218
    const char* name;       // 0x4, the row's type name, searched by fn_8018828C
    s32         count;      // 0x8, number of slots
    u32*        slots;      // 0xC, slots[i] holds a pointer to a 4-byte text record
};

/* A table of named rows, as an object of the Climax data layer. Name invented. */
struct unk_Table {
    u8              field_0x0[0x18];
    s16             count;  // 0x18, number of rows
    s16             field_0x1A;
    unk_TableRow*   rows;   // 0x1C, rows[i] is 16 bytes
};

extern "C" {
unk_TableRow*   fn_80188208( unk_Table* self, s32 index );
s32             fn_8018828C( unk_Table* self, const char* name, s32 start );
unk_TableRow*   fn_80188218( unk_Table* self, const char* key );
u32*            fn_80189D10( unk_TableRow* row, const char* field );
s32             fn_8000F8F8( unk_Table* self );
}

/* Packed name strings, each padded to a word: "PadRumbleData" at 0x0, "EventName" at 0x10, ... Defined in the unit of fn_8000FC5C. */
extern const char lbl_803DFC50[0x350];

#endif
