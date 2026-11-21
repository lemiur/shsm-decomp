// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <string.h>
#include <game/unk_80015598.h>

/*
====================
fn_80015598
====================
*/
// Fills the record pool of the tracker and links one slot to each record, one record per
// table entry. Each record keeps the running offset that the entries before it add up to.
// 0x80015598
extern "C" void fn_80015598( unk_Tracker* tracker ) {
    unk_Desc* desc = tracker->desc;
    u8* offset = tracker->field_0x00;
    unk_Record* record = desc->records;
    unk_Slot* slot;
    unk_TableEntry* entry;
    u32 i;
    u32 j;

    memset( desc->records, 0, desc->count * sizeof( unk_Record ) );
    tracker->field_0x14 = 0;
    slot = tracker->slots;
    fn_80015228( 1 );
    entry = lbl_8045A7E0;
    for ( i = 0; i < lbl_805291B8; i++ ) {
        for ( j = 0; j < entry->count; j++ ) {
            record->field_0x40 = entry->value;
            record->field_0x4C = offset;
            record->name[0] = 0;
            record->field_0x20 = 0;
            offset += entry->value;
            slot->record = record;
            record++;
            slot->name[0] = 0;
            slot->field_0x24 = 0;
            slot->field_0x28 = 0;
            slot->field_0x2C = 0;
            slot->field_0x30 = 0;
            slot->field_0x34 = 0;
            slot->field_0x38 = 0;
            slot++;
        }
        entry++;
    }
}

/*
====================
fn_80015840
====================
*/
// Event handler. A 0xD event sets the tracker's flag at +0x14 when the record pool matches
// fn_801B570C, and otherwise turns the event into 0xE and sets the flag at +0x15. A 0xE event
// initialises the tracker.
// 0x80015840
extern "C" int fn_80015840( int, unk_Event* event, unk_Tracker* tracker ) {
    if ( event->code == 0xD ) {
        int found = 0;
        if ( tracker != NULL ) {
            unk_Desc* desc = tracker->desc;
            unk_Record* records = desc->records;
            u8* base = tracker->field_0x00 - 0x10;
            void* expected = desc->field_0x0C;
            if ( expected == NULL ) {
                found = 1;
            } else {
                found = fn_801B570C( records, base ) == expected;
            }
        }
        if ( !found ) {
            event->code = 0xE;
            tracker->field_0x15 = 1;
        } else {
            tracker->field_0x14 = 1;
        }
    } else if ( event->code == 0xE ) {
        fn_80015598( tracker );
    }
    return 0;
}

/*
====================
fn_8001568C
====================
*/
// Forwards to the callback when one is set. The result of the callback is not used.
// 0x8001568C
extern "C" int fn_8001568C( int a, unk_Event* event, unk_Tracker* tracker ) {
    if ( tracker->callback != NULL ) {
        tracker->callback( a, event, tracker->context );
    }
    return 0;
}

/*
====================
fn_80015904
====================
*/
// 0x80015904
extern "C" int fn_80015904() {
    return 0;
}

/*
====================
fn_800156C4
====================
*/
// On a 0xD event sets the tracker's flag at +0x14. On a 0xE event copies each slot's name
// into its record and clears the name.
// Wall (register/schedule): control flow matches; retail keeps the byte offset and the slot
// pointer in the other two registers.
// 0x800156C4
extern "C" int fn_800156C4( int, unk_Event* event, unk_Tracker* tracker ) {
    if ( event->code == 0xD ) {
        tracker->field_0x14 = 1;
    } else if ( event->code == 0xE ) {
        for ( u32 i = 0; i < tracker->desc->count; i++ ) {
            unk_Slot* slot = &tracker->slots[i];
            if ( slot->name[0] != '\0' ) {
                strcpy( slot->record->name, slot->name );
                slot->record->field_0x44 = slot->field_0x24;
                slot->name[0] = '\0';
            }
        }
    }
    return 0;
}

/*
====================
fn_80015768
====================
*/
// On a 0xD event, checks the record pool against the owner's value at +0x44 and turns the
// event into 0x8 when they differ. Then forwards the event to the callback.
// Wall (register/schedule): retail loads the base out-parameter before the owner's +0x44 load,
// which the source order here does not produce.
// 0x80015768
extern "C" int fn_80015768( int a, unk_Event* event, unk_Tracker* tracker ) {
    if ( event->code == 0xD ) {
        int found = 0;
        unk_Record* records = NULL;
        u8* base = NULL;

        fn_80014B44( &records, &base );
        if ( records != NULL ) {
            unk_Owner* owner = (unk_Owner*)tracker->field_0x00;
            void* expected = owner->field_0x44;
            if ( expected == NULL ) {
                found = 1;
            } else {
                found = fn_801B570C( records, base ) == expected;
            }
        }
        if ( !found ) {
            event->code = 0x8;
        }
    }
    if ( tracker->callback != NULL ) {
        tracker->callback( a, event, tracker->context );
    }
    return 0;
}
