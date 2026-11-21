// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80085094.h>

/*
====================
fn_80085094

Stores the handle of the effect named by the strings, if the bank has it. Name invented.
====================
*/
// 0x80085094
extern "C" bool fn_80085094( u32* out, const char* name, const char* kind ) {
    if ( name != NULL && kind != NULL ) {
        unk_Effect* effect = fn_8008A934( lbl_805296E0, name, kind, 0, 1 );
        if ( effect->field_0x660 != 0 ) {
            u32 handle = effect->field_0x40;
            *out = handle;
            return handle != 0;
        }
    }
    return false;
}
