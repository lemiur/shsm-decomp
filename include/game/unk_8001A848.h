#ifndef GAME_UNK_8001A848_H
#define GAME_UNK_8001A848_H

#include <revolution/types.h>

// Placeholder types for the table that fn_8001A848 reads. Names invented: no class evidence yet.
// Each level of pointers is confirmed by the loads of fn_8001A848 (table -> slot -> target -> float).
struct unk_Target {
    u32 field_0x0;
    u32 field_0x4;
    u32 field_0x8;
    float field_0xC;
};

struct unk_TargetSlot {
    u32 field_0x0;
    u32 field_0x4;
    u32 field_0x8;
    unk_Target* target;
};

struct unk_SlotTable {
    unk_TargetSlot** slots;
    u32 field_0x4;
    u32 field_0x8;
    u32 count;
};

extern "C" {
// Reads the float of the target in slots[ index ], or 0.0f when index is out of range (0xFFFFFFFF
// included) or a link on the way is NULL.
float fn_8001A848( unk_SlotTable* table, u32 index );
void fn_8001A8C0();
}

#endif
