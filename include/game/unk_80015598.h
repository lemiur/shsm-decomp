#ifndef GAME_UNK_80015598_H
#define GAME_UNK_80015598_H

#include <revolution/types.h>
#include <game/unk_TableEntry.h>

// Types of this unit. Names and roles are invented from the retail accesses; the layouts
// are the offsets the code uses.

// Record (0x58 bytes), one per table entry, filled by fn_80015598.
struct unk_Record {
    char name[0x20];           // +0x00
    u8   field_0x20;           // +0x20
    u8   field_0x21[0x1F];     // +0x21
    u32  field_0x40;           // +0x40
    u32  field_0x44;           // +0x44
    u8   field_0x48[0x4];      // +0x48
    u8*  field_0x4C;           // +0x4C
    u8   field_0x50[0x8];      // +0x50
};

// Slot (0x3C bytes), one per record, at tracker->slots.
struct unk_Slot {
    unk_Record* record;        // +0x00
    char name[0x20];           // +0x04
    u32  field_0x24;           // +0x24
    u32  field_0x28;           // +0x28
    u32  field_0x2C;           // +0x2C
    u8   field_0x30;           // +0x30
    u8   field_0x31[0x3];      // +0x31
    u32  field_0x34;           // +0x34
    u32  field_0x38;           // +0x38
};

// Record pool at tracker->desc: its record count, and the records inline from +0x10.
struct unk_Desc {
    u32        field_0x00;     // +0x00
    u32        count;          // +0x04
    u8         field_0x08[0x4];// +0x08
    void*      field_0x0C;     // +0x0C
    unk_Record records[1];     // +0x10 (count elements)
};

// Event passed to the handlers. Only the code word is read by the functions of this unit.
struct unk_Event {
    s32 code;                  // +0x00 (0xD and 0xE are handled here)
};

// The object at tracker->field_0x00; only its field at +0x44 is read here (name invented).
struct unk_Owner {
    u8    field_0x00[0x44];    // +0x00
    void* field_0x44;          // +0x44
};

// Handler signature shared by the callbacks of this unit (name invented).
struct unk_Tracker;
typedef int (*unk_Callback)( int, unk_Event*, unk_Tracker* );

// The object the handlers take as their third argument (name invented). Its callback at
// +0x34 is called with the context at +0x38.
struct unk_Tracker {
    u8*          field_0x00;     // +0x00 (running offset base; its +0x44 is read by fn_80015768)
    u8           field_0x04[0x8];// +0x04
    unk_Desc*    desc;           // +0x0C
    unk_Slot*    slots;          // +0x10
    u8           field_0x14;     // +0x14 (flag)
    u8           field_0x15;     // +0x15 (flag)
    u8           field_0x16[0x1E];// +0x16
    unk_Callback callback;       // +0x34
    unk_Tracker* context;        // +0x38
};

extern "C" void fn_80015228( int );
extern "C" void fn_80014B44( unk_Record** records, u8** base );
extern "C" void* fn_801B570C( unk_Record* records, u8* base );

// Initialises the records and slots of a tracker from the table (name invented).
extern "C" void fn_80015598( unk_Tracker* tracker );

// Event handler: flags the tracker, or turns a 0xD event into 0xE; a 0xE event
// initialises the tracker (name invented).
extern "C" int fn_80015840( int, unk_Event* event, unk_Tracker* tracker );

// Event handler: copies the slot names into the records (name invented).
extern "C" int fn_800156C4( int, unk_Event* event, unk_Tracker* tracker );

// Event handler: checks the record pool on a 0xD event, then forwards to the callback
// (name invented).
extern "C" int fn_80015768( int a, unk_Event* event, unk_Tracker* tracker );

// Forwards to the callback at +0x34 when one is set (name invented).
extern "C" int fn_8001568C( int a, unk_Event* event, unk_Tracker* tracker );

extern "C" int fn_80015904();

#endif
