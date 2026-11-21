// Unit name: ZoneLink (class registry string in fn_8002D1A8); boundary: this range of its methods

#include <game/ZoneLink.h>

/*
====================
fn_8002C79C

Releases the two resource requests this zone link holds (the base is passed as the owner).
====================
*/
// 0x8002C79C
void ZoneLink::fn_8002C79C() {
    if ( field_0x3C4 != NULL ) {
        fn_801B0B8C( lbl_8052A5C0, field_0x3C4, this, 0 );
        field_0x3C4 = NULL;
    }
    if ( field_0x3C8 != NULL ) {
        fn_801B0B8C( lbl_8052A5C0, field_0x3C8, this, 0 );
        field_0x3C8 = NULL;
    }
}

/*
====================
fn_8002C824

First entry's first pointer.
====================
*/
// 0x8002C824
void* ZoneLink::fn_8002C824() {
    return entries[0].field_0x0;
}

/*
====================
fn_8002C82C

Current entry's first pointer, NULL without a current entry.
====================
*/
// 0x8002C82C
void* ZoneLink::fn_8002C82C() {
    if ( entryIndex >= 0 ) {
        return entries[entryIndex].field_0x0;
    }
    return NULL;
}

/*
====================
fn_8002C850

Current entry's second pointer, NULL without a current entry.
====================
*/
// 0x8002C850
void* ZoneLink::fn_8002C850() {
    if ( entryIndex >= 0 ) {
        return entries[entryIndex].field_0x4;
    }
    return NULL;
}

/*
====================
fn_8002C874

Field at 0x8 of the first base.
====================
*/
// 0x8002C874
u32 ZoneLink::fn_8002C874() {
    return field_0x8;
}

/*
====================
fn_8002C87C

Second byte of the current entry, 0 without a current entry.
====================
*/
// 0x8002C87C
u8 ZoneLink::fn_8002C87C() {
    if ( entryIndex >= 0 ) {
        return entries[entryIndex].field_0x10[1];
    }
    return 0;
}

/*
====================
fn_8002C8A0

Sets the state byte; a change with a resource held hands the old resource to fn_8009E894.
====================
*/
// 0x8002C8A0
void ZoneLink::fn_8002C8A0( u32 value ) {
    if ( field_0x39C == value ) {
        return;
    }
    void* old = field_0x130;
    field_0x39C = value;
    if ( old == NULL ) {
        return;
    }
    unk_ZoneLinkMid& base = *this;
    fn_8009E894( &base, old, value );
}
