// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8001590C.h>
#include <extras.h>
#include <string.h>

/*
====================
fn_80015E98

Finds the record whose name matches. A disabled registry reports to the manager and finds nothing.

Open (98.6%): register residue only. Retail keeps the record in r28 and the loop index in r31.
====================
*/
// 0x80015E98
extern "C" unk_EntryRecord* fn_80015E98( unk_EntryRegistry* self, const char* name ) {
    unk_EntryRecord* record = NULL;
    if ( self->disabled ) {
        unk_Manager* manager = fn_800149C8();
        unk_NotifyMessage message;
        memset( &message, 0, sizeof( message ) );
        strcpy( message.name, self->name );
        fn_80012E18( manager, &message, 8, 0 );
    }
    if ( self->disabled ) {
        return NULL;
    }
    u32 i = 0;
    unk_EntrySlot* found = NULL;
    for ( ; i < self->list->count && found == NULL; i++ ) {
        if ( stricmp( self->slots[i].record->name, name ) == 0 ) {
            found = &self->slots[i];
        }
    }
    if ( found != NULL ) {
        record = found->record;
    }
    return record;
}

/*
====================
fn_80015C10

Activates the slot named by the registry. A disabled registry reports to the manager and returns NULL.

Open (94.5%): register residue. Retail keeps this in r31 and the arguments in r23 to r27.
====================
*/
// 0x80015C10
extern "C" unk_EntrySlot* fn_80015C10( unk_EntryRegistry* self, const char* name, int unused0, int value, int unused1,
                                       u8 flag, int a, int b ) {
    if ( self->disabled ) {
        unk_Manager* manager = fn_800149C8();
        unk_NotifyMessage message;
        memset( &message, 0, sizeof( message ) );
        strcpy( message.name, self->name );
        fn_80012E18( manager, &message, 8, 0 );
    }
    if ( self->disabled ) {
        return NULL;
    }
    unk_EntrySlot* found = NULL;
    for ( u32 i = 0; i < self->list->count && found == NULL; i++ ) {
        if ( stricmp( self->slots[i].record->name, name ) == 0 ) {
            found = &self->slots[i];
        }
    }
    if ( found != NULL ) {
        found->field_0x28 = value;
        found->state = 2;
        found->field_0x34 = a;
        found->field_0x38 = b;
        found->flag = flag;
        fn_8001590C( self );
    }
    return found;
}

/*
====================
fn_80015D2C

Finds the slot named by the registry, or claims a free record with the same id for it. The new
slot takes the alias and the flag, then the registry is updated.

Open (94.8%): register residue. Retail keeps this in r31 and the arguments in r23 to r27.
====================
*/
// 0x80015D2C
extern "C" unk_EntrySlot* fn_80015D2C( unk_EntryRegistry* self, const char* name, const char* alias, int value, int id,
                                       u8 flag, int a, int b ) {
    int notify = 1;
    unk_EntrySlot* found = NULL;
    for ( u32 i = 0; i < self->list->count && found == NULL; i++ ) {
        if ( stricmp( self->slots[i].record->name, name ) == 0 ) {
            found = &self->slots[i];
        }
    }
    if ( found == NULL ) {
        for ( u32 i = 0; i < self->list->count && found == NULL; i++ ) {
            unk_EntryRecord* candidate = self->slots[i].record;
            if ( candidate->name[0] == 0 && candidate->id == id ) {
                found = &self->slots[i];
            }
        }
        // The pool is sized to hold every id, so a free record is always found here.
        strcpy( found->record->alias, alias );
        notify = 0;
    }
    if ( found != NULL ) {
        strcpy( found->name, found->record->name );
        found->field_0x24 = found->record->field_0x44;
        strcpy( found->record->name, name );
        found->field_0x28 = value;
        found->field_0x34 = a;
        found->field_0x38 = b;
        found->state = 1;
        found->flag = flag;
        if ( flag != 0 ) {
            fn_8001397C( fn_800149C8(), notify );
        }
        fn_8001590C( self );
    }
    return found;
}

/*
====================
fn_8001590C

Open: the body is not reconstructed. It starts with a registration block that reads the
fn_801B570C result through this->field_0x00 - 0x10, and then calls manager vtable slots 0x10,
0x14, 0x18 and 0x24, none of which is declared on unk_Manager yet. Its loop over the slots
(state 1 to fn_8001568C, state 2 to fn_80015768) is recorded in the notes, not written.
====================
*/
// 0x8001590C
extern "C" void fn_8001590C( unk_EntryRegistry* self ) {
    // TODO: reconstruct, see the header comment
    (void)self;
}
