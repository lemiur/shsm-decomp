#ifndef GAME_UNK_8000FC5C_H
#define GAME_UNK_8000FC5C_H

#include <revolution/types.h>

/* Names invented; layouts as read by fn_800105B8. */
struct unk_EventLookupEntry {
    u8      field_0x00[0x4];
    void**  values;             // 0x4, indexed by the node's halfword at 0x8; entries are 8 bytes
};

struct unk_EventLookup {
    unk_EventLookupEntry* entries;  // 0x0
};

/* The halfword at 0xE selects the lookup entry. Name invented. */
struct unk_EventValueKey {
    u8      field_0x00[0xE];
    s16     index;              // 0xE
};

/*
 * One event definition read from a node of the event document (the "Event", "Dir", "Type",
 * "D1", "D2", "Time" and "Time2" attributes that fn_8000FC5C reads). Type and role name
 * invented; the members are placed by the retail code of this unit.
 */
struct unk_EventDescriptor {
    u8      field_0x00[0x10];   // base part, not yet recovered
    u32     flags;              // 0x10, bits 0x1 and 0x2 read by fn_80010B78
    unk_EventLookup* lookup;    // 0x14, the value table fn_800105B8 reads
    s32     count;              // 0x18, entries in the array at 0x1c
    void**  entries;            // 0x1c
    s32     field_0x20;
    s32     field_0x24;         // 0x24, set by fn_80010B10
    s32     field_0x28;
    s32     field_0x2c;
    f32     field_0x30;
    s32     field_0x34;
    void*   child0;             // 0x38, presence checked by fn_80010B18
    void*   child1;             // 0x3c
    void*   child2;             // 0x40
    void*   child3;             // 0x44
    void*   child4;             // 0x48
    f32     field_0x4c;
    f32     field_0x50;
    f32     field_0x54;
    s32     eventCount;         // 0x58, children named "Event"
    s32     field_0x5c;         // 0x5c
    void**  field_0x60;         // 0x60, array of eventCount entries
    void**  field_0x64;         // 0x64, array of field_0x5c entries
};

/*
 * A node of the parsed event document as the readers of this unit use it. Name and type
 * invented; the members are the ones the retail code reads (name at 0x4, children at 0x14).
 */
struct unk_EventNode {
    u8              field_0x00[0x4];
    char*           name;           // 0x4, the element name compared with the "Event" family strings
    s16             field_0x8;      // 0x8
    u8              field_0xa[0x2];
    void*           attrs;          // 0xC, read by fn_80189D10
    u8              field_0x10[0x4];
    struct unk_EventChildren* children;  // 0x14
    u8              field_0x18[0x8];
    struct unk_EventValueKey* valueKey;  // 0x20
};

/* A child table: the count is a halfword at 0x18; fn_80189CCC reads the entries. Name invented. */
struct unk_EventChildren {
    u8              field_0x00[0x18];
    s16             count;          // 0x18
};

/* An entry returned by fn_80189CCC: its node is at 0x4. Name invented. */
struct unk_EventElem {
    u8              field_0x00[0x4];
    unk_EventNode*  node;           // 0x4
};

extern "C" {
void fn_8000FC5C();
s32 fn_800103EC(unk_EventDescriptor* self, unk_EventChildren* children);
s32 fn_800105B8(unk_EventDescriptor* self, unk_EventChildren* children, s32 index);
unk_EventElem* fn_80189CCC(unk_EventChildren* children, s32 index);  // defined in auto_03_80186E80_text, unreconstructed
void fn_80010784();
void fn_80010B10(unk_EventDescriptor* self, s32 value);
void* fn_80010B18(unk_EventDescriptor* self, s32 index);
s32 fn_80010B78(unk_EventDescriptor* self);
void fn_80010B94();
void fn_80010E28();
void fn_80010E2C();
void* fn_80010FC4(unk_EventDescriptor* self, void* key);
s32 fn_800114CC(void* entry, void* key);    // defined in the unreconstructed auto_03_80011054_text
void* fn_80010FC4(unk_EventDescriptor* self, void* key);
}

#endif
