// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8000F8F8.h>
#include <string.h>

/* The bytes a text field takes in its row: its length plus a terminator, rounded up to a word; 0 when the row or the field is absent. */
#define UNK_TEXT_SIZE( row, field )                                                     \
    ( ( row ) == NULL ? 0                                                               \
      : ( ( slot = fn_80189D10( row, field ) ) == NULL || *slot + 4 == 0 ? 0            \
          : ( ( strlen( (const char*)( *slot + 4 ) ) + 4 ) & ~3 ) ) )

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata 0x80526820 size 0x6 lbl_80526820
//   .sdata 0x80526828 size 0x6 lbl_80526828

/*
====================
fn_8000F8F8
====================
*/
// Size in bytes of the packed block for the table's events, pads and animation rows.
// Wall (wall.exe, 64%): register/schedule (the table base and self swap registers), control flow
// (the optional text-field tests compile to extra tails), referent alignment after the event loop.
// 0x8000F8F8
extern "C" s32 fn_8000F8F8( unk_Table* self ) {
    s32             size;
    s32             eventCount;
    s32             padCount;
    s32             i;
    s32             k;
    unk_TableRow*   row;
    u32*            slot;
    const char*     names;

    names = lbl_803DFC50;
    size = 0x68;
    eventCount = 0;
    padCount = 0;
    for ( i = 0; i < self->count; i++ ) {
        const char* name = fn_80188208( self, i )->name;
        if ( strcmp( name, "Event" ) == 0 ) {
            eventCount++;
        } else if ( strcmp( name, names + 0 ) == 0 ) {
            padCount++;
        }
    }

    if ( eventCount > 0 ) {
        size = 0x68 + eventCount * 4 + eventCount * 0x1c;
        i = 0;
        for ( k = 0; k < eventCount; k++ ) {
            i = fn_8018828C( self, "Event", i );
            row = fn_80188208( self, i );
            size += UNK_TEXT_SIZE( row, names + 0x10 );
            i++;
        }
    }

    if ( padCount > 0 ) {
        size += padCount * 4 + padCount * 0x14;
    }

    row = fn_80188218( self, names + 0x28 );
    size += UNK_TEXT_SIZE( row, names + 0x1c );

    row = fn_80188218( self, names + 0x40 );
    size += UNK_TEXT_SIZE( row, names + 0x34 );

    row = fn_80188218( self, names + 0x4c );
    if ( row == NULL ) {
        return size;
    }
    size += UNK_TEXT_SIZE( row, names + 0x64 );
    size += UNK_TEXT_SIZE( row, names + 0x70 );
    size += UNK_TEXT_SIZE( row, names + 0x80 );
    size += UNK_TEXT_SIZE( row, names + 0x8c );
    return size;
}
