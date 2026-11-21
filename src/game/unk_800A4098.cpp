// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800A4098.h>
#include <game/unk_800A40B4.h>

/*
====================
fn_800A4098
====================
*/
// 0x800A4098
extern "C" unk_RpAtomic* fn_800A4098( unk_RpAtomic* atomic, void* data ) {
    unk_800A40B4* self = static_cast< unk_800A40B4* >( data );
    void* geometry = atomic->geometry;

    /* An atomic without a geometry is skipped; the first one with a geometry is recorded and ends the walk. */
    if( geometry == NULL ) {
        return atomic;
    }
    self->field_0x1dc = atomic;
    self->field_0x1e0 = geometry;
    return NULL;
}
