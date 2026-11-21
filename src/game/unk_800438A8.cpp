// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800438A8.h>

/*
====================
fn_800438A8

Resolves the object's cached lookup (key at 0x14C, table at 0x99C, value at 0x9A0), caching the
result in 0x9A4 with its key in 0x9A8. Sets the flag at 0x20 of the 0x9FC holder when the lookup
yields no value. The first argument is unused.
====================
*/
// 0x800438A8
extern "C" void fn_800438A8( void* unused, unk_LookupCacheOwner* self ) {
    void* key;
    void* value;
    unk_FlagHolder* holder;
    unk_LookupRecord* record;

    if ( self == NULL ) {
        return;
    }
    holder = self->field_0x9fc;
    if ( holder == NULL ) {
        return;
    }
    key = self->field_0x14c;
    if ( key == self->field_0x9a8 ) {
        record = self->field_0x9a4;
    } else {
        record = NULL;
    }
    if ( record == NULL ) {
        record = fn_800A2A18( key, self->field_0x99c, self->field_0x9a0 );
        self->field_0x9a4 = record;
        self->field_0x9a8 = key;
    }
    value = key;
    if ( record != NULL ) {
        value = record->field_0x10;
    }
    if ( value == NULL ) {
        holder->field_0x20 = 1;
    }
}

/*
====================
fn_8004394C
====================
*/
// 0x8004394C
extern "C" void fn_8004394C() {
}
