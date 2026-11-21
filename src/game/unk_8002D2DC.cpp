// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8002D2DC.h>

/*
====================
fn_8002D2DC

Load callback of the zone manager's request: loads the named asset again and, when it is in
memory, records its resource and releases the request's handle.
====================
*/
// 0x8002D2DC
// Identity TODO: retail converts lbl_80529260 to its unk_SpawnerResource base, which sits at offset
// 8 of unk_RespotManager (a derived-to-base conversion with a null check). The layout of the
// bytes before that base is not known yet, so the owner below keeps the pointer unchanged.
// Parked: the unit stays NonMatching until the base is identified.
extern "C" void fn_8002D2DC( void* handle, u32 unused, const char* name ) {
    void* resource = fn_801B1E3C( lbl_8052A5C0, name, (unk_SpawnerResource*)lbl_80529260, 0 );
    if ( resource == NULL ) {
        return;
    }
    lbl_80529264 = fn_801A8554( name, resource );
    fn_801B2004( lbl_8052A5C0, resource );
    fn_801B0B8C( lbl_8052A5C0, handle, (unk_SpawnerResource*)lbl_80529260, 0 );
}
