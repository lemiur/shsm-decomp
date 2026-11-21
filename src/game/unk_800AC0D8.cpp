// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
// Evidence for the classes: the unit's .data string table holds the cutscene (IGC)
// debug strings ("Could not allocate IGC streaming buffer!", "Overall CutScene, Duration").

#include <game/unk_800AC0D8.h>

// vtable of unk_800AC5BC: offset-to-top and typeinfo (both null), then the virtual destructor.
extern "C" void* lbl_803EA0E8[4] = { 0, 0, (void*)fn_800AC5BC, 0 };

/*
====================
fn_800AC0D8

Constructor of the 0x30-byte element of unk_800AC24C: no pointers, id -1, default floats.
====================
*/
// 0x800AC0D8
extern "C" void fn_800AC0D8( unk_800AC0D8* self ) {
    self->m_pIndices = 0;
    self->m_count = 0;
    self->m_id = UNK_ID_UNSET;
    self->m_f14 = 0.0f;
    self->m_f18 = -1.0f;
    self->m_flags = 0;
    self->m_f24 = 0.0f;
    self->m_pObj0C = 0;
    self->m_pObj20 = 0;
    self->m_pObj1C = 0;
    self->m_n28 = 5;
    self->m_b2C = 0;
    self->m_b2D = 0;
}

/*
====================
fn_800AC128

Destructor of the element of unk_800AC24C (TODO: body not yet reconstructed; the retail code
deletes m_pObj0C and m_pObj20 through their vtables and frees m_pObj1C and m_pIndices).
====================
*/
// 0x800AC128
extern "C" void fn_800AC128( unk_800AC0D8* self, s32 flags ) {
    // TODO
}

/*
====================
fn_800AC24C

Loads unk_800AC24C from a stream (TODO: body not yet reconstructed).
====================
*/
// 0x800AC24C
extern "C" void fn_800AC24C() {
    // TODO
}

/*
====================
fn_800AC518

Constructor of unk_800AC518: zeroes the members, sets the embedded list's vtable and empties the list.
====================
*/
// 0x800AC518
extern "C" void fn_800AC518( unk_800AC518* self ) {
    self->m_list.__vptr = lbl_803EA0E8;
    self->m_list.m_count = 0;
    self->m_list.m_pLast = 0;
    self->m_list.m_pFirst = 0;
    self->m_u724 = 0;
    self->field_0x004 = 0;
    self->m_pOwner = 0;
    self->field_0x6E4 = 0;
    self->m_pSet = 0;
    self->field_0x6EC = 0;
    self->field_0x6F0 = 0;
    self->field_0x6F4 = 0;
    self->field_0x6F8 = 0;
    self->m_b704 = 0;
    self->field_0x708 = 0;
    self->field_0x70C[0] = 0;
    self->field_0x70C[1] = 0;
    self->field_0x70C[2] = 0;
    self->field_0x70C[3] = 0;
    self->m_bCC = 0;
    self->field_0x70C[4] = 0;
    self->field_0x70C[5] = 0;
    self->field_0x70C[6] = 0;
    self->field_0x714 = 0;
    self->m_entryCount = 0;
    self->m_f6DC = 1.0f;
    self->m_b4C = 0;
    self->m_bD4 = 0;
    self->m_f6E0 = 0.0f;
    self->m_f718 = 0.0f;
    self->field_0x70C[7] = 0;
    self->field_0x71C = 0;
    self->field_0x6E8 = 0;
    self->m_u720 = 1500;
}

/*
====================
fn_800AC5BC

Destructor of unk_800AC5BC: frees every node, then deletes the object when flags is positive.
====================
*/
// 0x800AC5BC
extern "C" unk_800AC5BC* fn_800AC5BC( unk_800AC5BC* self, s32 flags ) {
    unk_800AC5BCNode* node;

    if ( self != 0 ) {
        self->__vptr = lbl_803EA0E8;
        while ( ( node = (unk_800AC5BCNode*)self->m_pLast ) != 0 ) {
            if ( node->m_pNext == 0 ) {
                self->m_pLast = node->m_pPrev;
            } else {
                node->m_pNext->m_pPrev = node->m_pPrev;
            }
            if ( node->m_pPrev == 0 ) {
                self->m_pFirst = node->m_pNext;
            } else {
                node->m_pPrev->m_pNext = node->m_pNext;
            }
            node->m_pNext = 0;
            node->m_pPrev = 0;
            self->m_count--;
            delete node;
        }
        if ( flags > 0 ) {
            delete self;
        }
    }
    return self;
}

/*
====================
fn_800AC694

Not reconstructed: the body inlines the list destructor (fn_800AC5BC's body with no delete) and calls
fn_800ACD0C and fn_800AF050 (another unit), so the inlined list teardown needs an inline destructor
in the header before this body can match.
====================
*/
// 0x800AC694
extern "C" void fn_800AC694() {
    // TODO
}

/*
====================
fn_800AC7AC

TODO: not yet reconstructed.
====================
*/
// 0x800AC7AC
extern "C" void fn_800AC7AC() {
    // TODO
}

/*
====================
fn_800AC944

TODO: not yet reconstructed.
====================
*/
// 0x800AC944
extern "C" void fn_800AC944() {
    // TODO
}

/*
====================
fn_800ACAC4

Returns the flag byte of the owner at +0x8, or 1 when there is no owner.
====================
*/
// 0x800ACAC4
extern "C" u8 fn_800ACAC4( unk_800AC518* self ) {
    if ( self->m_pOwner == 0 ) {
        return 1;
    }
    return self->m_pOwner->m_flagB4;
}

/*
====================
fn_800ACAE0

Inserts an entry into the table sorted by key: the first slot whose key is not below the new key,
after shifting the tail up by one. Returns the slot, or -1 when the table is full (127 entries).
Not exact: control flow wall. Retail's shift loop carries an extra test on the index that keeps MWCC
from unrolling it; the counted form is unrolled by eight.
====================
*/
// 0x800ACAE0
extern "C" s32 fn_800ACAE0( unk_800AC518* self, float key, void* a, void* b ) {
    unk_800AC518Entry* entry;
    s32 i = 0;
    s32 j;

    entry = self->m_entries;
    while ( key > entry->m_key && (u32)i < self->m_entryCount ) {
        entry++;
        i++;
    }
    if ( self->m_entryCount == 127 ) {
        return -1;
    }
    entry = &self->m_entries[self->m_entryCount - 1];
    for ( j = self->m_entryCount - 1; j >= i; j-- ) {
        entry[1].m_key = entry[0].m_key;
        entry[1].m_a = entry[0].m_a;
        entry[1].m_b = entry[0].m_b;
        entry--;
    }
    self->m_entries[i].m_key = key;
    self->m_entries[i].m_a = a;
    self->m_entries[i].m_b = b;
    self->m_entryCount++;
    return i;
}

/*
====================
fn_800ACB94

Not reconstructed: walls on the element's polymorphic object at +0x20 (class unknown; the call is
virtual slot 19 after the two destructor slots, with one argument), so no typed call can be written yet.
====================
*/
// 0x800ACB94
extern "C" void fn_800ACB94() {
    // TODO
}

/*
====================
fn_800ACC2C

Empty function.
====================
*/
// 0x800ACC2C
extern "C" void fn_800ACC2C() {
}

/*
====================
fn_800ACC30

TODO: not yet reconstructed.
====================
*/
// 0x800ACC30
extern "C" void fn_800ACC30() {
    // TODO
}

/*
====================
fn_800ACD0C

TODO: not yet reconstructed.
====================
*/
// 0x800ACD0C
extern "C" void fn_800ACD0C() {
    // TODO
}

/*
====================
fn_800ACF18

TODO: not yet reconstructed.
====================
*/
// 0x800ACF18
extern "C" void fn_800ACF18() {
    // TODO
}

/*
====================
fn_800AD14C

TODO: not yet reconstructed.
====================
*/
// 0x800AD14C
extern "C" void fn_800AD14C() {
    // TODO
}

/*
====================
fn_800AD2BC

TODO: not yet reconstructed.
====================
*/
// 0x800AD2BC
extern "C" void fn_800AD2BC() {
    // TODO
}

/*
====================
fn_800AD3DC

TODO: not yet reconstructed.
====================
*/
// 0x800AD3DC
extern "C" void fn_800AD3DC() {
    // TODO
}

/*
====================
fn_800AD42C

TODO: not yet reconstructed.
====================
*/
// 0x800AD42C
extern "C" void fn_800AD42C() {
    // TODO
}

/*
====================
fn_800AD558

TODO: not yet reconstructed.
====================
*/
// 0x800AD558
extern "C" void fn_800AD558() {
    // TODO
}

/*
====================
fn_800AD670

TODO: not yet reconstructed.
====================
*/
// 0x800AD670
extern "C" void fn_800AD670() {
    // TODO
}

/*
====================
fn_800AD6F8

TODO: not yet reconstructed.
====================
*/
// 0x800AD6F8
extern "C" void fn_800AD6F8() {
    // TODO
}

/*
====================
fn_800AD700

TODO: not yet reconstructed.
====================
*/
// 0x800AD700
extern "C" void fn_800AD700() {
    // TODO
}

/*
====================
fn_800AD708

TODO: not yet reconstructed.
====================
*/
// 0x800AD708
extern "C" void fn_800AD708() {
    // TODO
}

/*
====================
fn_800AD710

TODO: not yet reconstructed.
====================
*/
// 0x800AD710
extern "C" void fn_800AD710() {
    // TODO
}

/*
====================
fn_800AD80C

TODO: not yet reconstructed.
====================
*/
// 0x800AD80C
extern "C" void fn_800AD80C() {
    // TODO
}

/*
====================
fn_800AE244

TODO: not yet reconstructed.
====================
*/
// 0x800AE244
extern "C" void fn_800AE244() {
    // TODO
}

/*
====================
fn_800AE24C

TODO: not yet reconstructed.
====================
*/
// 0x800AE24C
extern "C" void fn_800AE24C() {
    // TODO
}

/*
====================
fn_800AE258

TODO: not yet reconstructed.
====================
*/
// 0x800AE258
extern "C" void fn_800AE258() {
    // TODO
}

/*
====================
fn_800AE2A8

TODO: not yet reconstructed.
====================
*/
// 0x800AE2A8
extern "C" void fn_800AE2A8() {
    // TODO
}

/*
====================
fn_800AE2B0

TODO: not yet reconstructed.
====================
*/
// 0x800AE2B0
extern "C" void fn_800AE2B0() {
    // TODO
}

/*
====================
fn_800AE2B8

TODO: not yet reconstructed.
====================
*/
// 0x800AE2B8
extern "C" void fn_800AE2B8() {
    // TODO
}

/*
====================
fn_800AE2D8

TODO: not yet reconstructed.
====================
*/
// 0x800AE2D8
extern "C" void fn_800AE2D8() {
    // TODO
}

/*
====================
fn_800AE3A8

TODO: not yet reconstructed.
====================
*/
// 0x800AE3A8
extern "C" void fn_800AE3A8() {
    // TODO
}

/*
====================
fn_800AE498

TODO: not yet reconstructed.
====================
*/
// 0x800AE498
extern "C" void fn_800AE498() {
    // TODO
}

/*
====================
fn_800AE4A4

TODO: not yet reconstructed.
====================
*/
// 0x800AE4A4
extern "C" void fn_800AE4A4() {
    // TODO
}

/*
====================
fn_800AE550

TODO: not yet reconstructed.
====================
*/
// 0x800AE550
extern "C" void fn_800AE550() {
    // TODO
}

/*
====================
fn_800AE558

TODO: not yet reconstructed.
====================
*/
// 0x800AE558
extern "C" void fn_800AE558() {
    // TODO
}

/*
====================
fn_800AE610

TODO: not yet reconstructed.
====================
*/
// 0x800AE610
extern "C" void fn_800AE610() {
    // TODO
}

/*
====================
fn_800AE6F8

TODO: not yet reconstructed.
====================
*/
// 0x800AE6F8
extern "C" void fn_800AE6F8() {
    // TODO
}

/*
====================
fn_800AEF30

TODO: not yet reconstructed.
====================
*/
// 0x800AEF30
extern "C" void fn_800AEF30() {
    // TODO
}
