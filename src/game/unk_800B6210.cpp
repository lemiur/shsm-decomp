// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800B6210.h>

#include <string.h>

/*
====================
unk_800B6210::unk_800B6210 (fn_800B6210)
====================
*/
// Constructor body (caller not checked): zeroes the table, then allocates five owned objects.
// Open residue: retail stores the first object's vtable pointer at +0x10, not +0; the class layout
// that puts it there is not recovered (see the placeholder class unk_803EA6A0).
// 0x800B6210
unk_800B6210::unk_800B6210() {
    m_count = 0;
    memset( m_slots, 0, sizeof( m_slots ) );

    unk_803EA6A0* first = new ( 2, OBJECT_ALLOC_FLAGS ) unk_803EA6A0( 0, 1 );
    AddOwned( first );

    unk_800B1754* second = new ( 2, OBJECT_ALLOC_FLAGS ) unk_800B1754();
    AddOwned( second );

    unk_800B2E58* third = new ( 2, OBJECT_ALLOC_FLAGS ) unk_800B2E58();
    AddOwned( third );

    unk_800B63C8* fourth = new ( 2, OBJECT_ALLOC_FLAGS ) unk_800B63C8();
    AddOwned( fourth );

    unk_800B6520* fifth = new ( 2, OBJECT_ALLOC_FLAGS ) unk_800B6520();
    AddOwned( fifth );
}

/*
====================
unk_800B6210::fn_800B63BC
====================
*/
// 0x800B63BC
void* unk_800B6210::fn_800B63BC( s32 index ) {
    return m_slots[index].obj;
}
