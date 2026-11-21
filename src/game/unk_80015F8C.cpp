// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80015F8C.h>
#include <extras.h>
#include <string.h>

/*
====================
fn_80015F8C
====================
*/
// 0x80015F8C
// NonMatching, 91% exact: wall is register/schedule (the index and entry registers are swapped,
// and the call reloads lbl_8052A5C0 where retail reuses the stored value).
extern "C" bool fn_80015F8C( unk_NameTable* table, const char* name, u8 flag, void* ctxA, void* ctxB ) {
    u32 i;
    unk_NameEntry* entry = 0;

    for ( i = 0; i < table->header->count && entry == 0; i++ ) {
        if ( stricmp( table->entries[i].desc->name, name ) == 0 ) {
            entry = &table->entries[i];
        }
    }
    if ( entry != 0 ) {
        strcpy( entry->name, entry->desc->name );
        entry->field_0x24 = entry->desc->field_0x44;
        entry->desc->name[0] = 0;
        entry->field_0x28 = lbl_8052A5C0->field_0x0;
        entry->field_0x34 = ctxA;
        entry->field_0x38 = ctxB;
        entry->field_0x2c = 1;
        entry->field_0x30 = flag;
        fn_8001590C( table, lbl_8052A5C0->field_0x0, 0, ctxA );
    }
    return entry != 0;
}
