// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8005549C.h>

/*
====================
fn_8005549C
====================
*/
// 0x8005549C
// Resolves the lookup at 0xBB0 in the owner's table and flags the record at 0x1174 when the
// result is missing (a null entry with a null table).
void fn_8005549C(void* ctx, unk_LookupOwner* owner) {
    if (owner == NULL) return;
    unk_NamedTable* table;
    unk_LookupFlag* flag = owner->field_0x1174;
    if (flag == NULL) return;
    table = owner->table;
    unk_LookupEntry* entry;
    if (table == owner->field_0xBB0.table) {
        entry = owner->field_0xBB0.entry;
    } else {
        entry = NULL;
    }
    if (entry == NULL) {
        entry = fn_800A2A18(table, owner->field_0xBB0.name, owner->field_0xBB0.id);
        owner->field_0xBB0.entry = entry;
        owner->field_0xBB0.table = table;
    }
    s32 value;
    if (entry != NULL) {
        value = entry->value;
    } else {
        value = (s32)table;
    }
    if (value == 0) flag->flag = 1;
}

/*
====================
fn_80055540
====================
*/
// 0x80055540
// Registered as the second callback of fn_8005549C's node: nothing to do.
void fn_80055540(void* ctx, unk_LookupOwner* owner) {}

/*
====================
fn_80055544
====================
*/
// 0x80055544
// Resolves the lookup at 0x10C0 against the flag record at 0x1180 (same shape as fn_8005549C).
void fn_80055544(void* ctx, unk_LookupOwner* owner) {
    if (owner == NULL) return;
    unk_NamedTable* table;
    unk_LookupFlag* flag = owner->field_0x1180;
    if (flag == NULL) return;
    table = owner->table;
    unk_LookupEntry* entry;
    if (table == owner->field_0x10C0.table) {
        entry = owner->field_0x10C0.entry;
    } else {
        entry = NULL;
    }
    if (entry == NULL) {
        entry = fn_800A2A18(table, owner->field_0x10C0.name, owner->field_0x10C0.id);
        owner->field_0x10C0.entry = entry;
        owner->field_0x10C0.table = table;
    }
    s32 value;
    if (entry != NULL) {
        value = entry->value;
    } else {
        value = (s32)table;
    }
    if (value == 0) flag->flag = 1;
}

/*
====================
fn_800555E8
====================
*/
// 0x800555E8
// Registered as the second callback of fn_80055544's node: nothing to do.
void fn_800555E8(void* ctx, unk_LookupOwner* owner) {}

/*
====================
fn_800555EC
====================
*/
// 0x800555EC
// Resolves the lookup at 0xD60 against the flag record at 0x1178 and flags it when the result
// is missing.
void fn_800555EC(void* ctx, unk_LookupOwner* owner) {
    if (owner == NULL) return;
    unk_NamedTable* table;
    unk_LookupFlag* flag = owner->field_0x1178;
    if (flag == NULL) return;
    table = owner->table;
    unk_LookupEntry* entry;
    if (table == owner->field_0xD60.table) {
        entry = owner->field_0xD60.entry;
    } else {
        entry = NULL;
    }
    if (entry == NULL) {
        entry = fn_800A2A18(table, owner->field_0xD60.name, owner->field_0xD60.id);
        owner->field_0xD60.entry = entry;
        owner->field_0xD60.table = table;
    }
    s32 value;
    if (entry != NULL) {
        value = entry->value;
    } else {
        value = (s32)table;
    }
    if (value == 0) flag->flag = 1;
}

/*
====================
fn_80055690
====================
*/
// 0x80055690
// Registered as the second callback of fn_800555EC's node: nothing to do.
void fn_80055690(void* ctx, unk_LookupOwner* owner) {}

/*
====================
fn_80055694
====================
*/
// 0x80055694
// Resolves the lookup at 0xD60 against the flag record at 0x1178 and flags it when the result
// is 1.
void fn_80055694(void* ctx, unk_LookupOwner* owner) {
    if (owner == NULL) return;
    unk_NamedTable* table;
    unk_LookupFlag* flag = owner->field_0x1178;
    if (flag == NULL) return;
    table = owner->table;
    unk_LookupEntry* entry;
    if (table == owner->field_0xD60.table) {
        entry = owner->field_0xD60.entry;
    } else {
        entry = NULL;
    }
    if (entry == NULL) {
        entry = fn_800A2A18(table, owner->field_0xD60.name, owner->field_0xD60.id);
        owner->field_0xD60.entry = entry;
        owner->field_0xD60.table = table;
    }
    s32 value;
    if (entry != NULL) {
        value = entry->value;
    } else {
        value = (s32)table;
    }
    if (value == 1) flag->flag = 1;
}

/*
====================
fn_80055738
====================
*/
// 0x80055738
// Registered as the second callback of fn_80055694's node: nothing to do.
void fn_80055738(void* ctx, unk_LookupOwner* owner) {}

/*
====================
fn_8005573C
====================
*/
// 0x8005573C
// Resolves the lookup at 0xF10 against the flag record at 0x117C and flags it when the result
// is missing.
void fn_8005573C(void* ctx, unk_LookupOwner* owner) {
    if (owner == NULL) return;
    unk_NamedTable* table;
    unk_LookupFlag* flag = owner->field_0x117C;
    if (flag == NULL) return;
    table = owner->table;
    unk_LookupEntry* entry;
    if (table == owner->field_0xF10.table) {
        entry = owner->field_0xF10.entry;
    } else {
        entry = NULL;
    }
    if (entry == NULL) {
        entry = fn_800A2A18(table, owner->field_0xF10.name, owner->field_0xF10.id);
        owner->field_0xF10.entry = entry;
        owner->field_0xF10.table = table;
    }
    s32 value;
    if (entry != NULL) {
        value = entry->value;
    } else {
        value = (s32)table;
    }
    if (value == 0) flag->flag = 1;
}

/*
====================
fn_800557E0
====================
*/
// 0x800557E0
// Registered as the second callback of fn_8005573C's node: nothing to do.
void fn_800557E0(void* ctx, unk_LookupOwner* owner) {}

/*
====================
fn_800557E4
====================
*/
// 0x800557E4
// Resolves the lookup at 0xF10 against the flag record at 0x117C and flags it when the result
// is 1.
void fn_800557E4(void* ctx, unk_LookupOwner* owner) {
    if (owner == NULL) return;
    unk_NamedTable* table;
    unk_LookupFlag* flag = owner->field_0x117C;
    if (flag == NULL) return;
    table = owner->table;
    unk_LookupEntry* entry;
    if (table == owner->field_0xF10.table) {
        entry = owner->field_0xF10.entry;
    } else {
        entry = NULL;
    }
    if (entry == NULL) {
        entry = fn_800A2A18(table, owner->field_0xF10.name, owner->field_0xF10.id);
        owner->field_0xF10.entry = entry;
        owner->field_0xF10.table = table;
    }
    s32 value;
    if (entry != NULL) {
        value = entry->value;
    } else {
        value = (s32)table;
    }
    if (value == 1) flag->flag = 1;
}

/*
====================
fn_80055888
====================
*/
// 0x80055888
// Registered as the second callback of fn_800557E4's node: nothing to do.
void fn_80055888(void* ctx, unk_LookupOwner* owner) {}

/*
====================
fn_8005588C
====================
*/
// 0x8005588C
// Clears the flag at 0x15D3 and sets the flag at 0x15D1 when the key is the one at 0x15D4.
void fn_8005588C(void* ctx, u32 key, unk_LookupOwner* owner) {
    if (owner == NULL) return;
    owner->field_0x15D3 = 0;
    if (key != owner->field_0x15D4) return;
    owner->field_0x15D1 = 1;
}
