#ifndef GAME_UNK_8001590C_H
#define GAME_UNK_8001590C_H

#include <revolution/types.h>

/* Manager singleton that fn_800149C8 returns (unit at 0x800129E0 not yet reconstructed). Name invented. */
struct unk_Manager;

/* Message block sent to the manager. Names and layout invented. */
struct unk_NotifyMessage {
    int     field_0x00;     // +0x00, zeroed before the send
    int     field_0x04;     // +0x04, zeroed before the send
    int     field_0x08;     // +0x08
    char    name[0x9C];     // +0x0C, the registry's name

    unk_NotifyMessage() {
        field_0x00 = 0;
        field_0x04 = 0;
    }
};

/* A pooled record: its name is the lookup key. Names and layout invented. */
struct unk_EntryRecord {
    char    name[0x20];     // +0x00
    char    alias[0x20];    // +0x20
    int     id;             // +0x40
    int     field_0x44;     // +0x44
};

/* One slot of the registry array, 0x3C bytes. Names and layout invented. */
struct unk_EntrySlot {
    unk_EntryRecord*    record;         // +0x00
    char                name[0x20];     // +0x04
    int                 field_0x24;     // +0x24
    int                 field_0x28;     // +0x28
    int                 state;          // +0x2C: 0 free, 1 or 2 in use
    u8                  flag;           // +0x30
    int                 field_0x34;     // +0x34
    int                 field_0x38;     // +0x38
};

/* The list header at the registry's +0x0C. Names and layout invented. */
struct unk_EntryList {
    int     field_0x00;     // +0x00
    int     count;          // +0x04
    int     field_0x08;     // +0x08
    int     field_0x0C;     // +0x0C
};

/* The registry the four functions work on. Name invented. */
struct unk_EntryRegistry {
    void*           field_0x00;     // +0x00
    int             field_0x04;     // +0x04
    const char*     name;           // +0x08
    unk_EntryList*  list;           // +0x0C
    unk_EntrySlot*  slots;          // +0x10
    u8              field_0x14;     // +0x14
    u8              disabled;       // +0x15: notify, then ignore lookups
};

// defined in 0x800149C8, unit not yet reconstructed
unk_Manager* fn_800149C8();
// defined in 0x80012E18, unit not yet reconstructed. Argument names invented.
void         fn_80012E18( unk_Manager* manager, unk_NotifyMessage* message, int type, int flags );
// defined in 0x8001397C, unit not yet reconstructed
void         fn_8001397C( unk_Manager* manager, int value );

extern "C" {
unk_EntryRecord* fn_80015E98( unk_EntryRegistry* self, const char* name );
unk_EntrySlot*   fn_80015C10( unk_EntryRegistry* self, const char* name, int unused0, int value, int unused1,
                              u8 flag, int a, int b );
unk_EntrySlot*   fn_80015D2C( unk_EntryRegistry* self, const char* name, const char* alias, int value, int id,
                              u8 flag, int a, int b );
void             fn_8001590C( unk_EntryRegistry* self );
}

#endif
