// Unit name and boundary: the class name is the registration string "CGenericCar"; the boundary
// is the single-function range the unit_plan proposed.

#include <game/CGenericCar.h>

// lbl_8052721C is defined by the sdata unit that holds it (not reconstructed); this unit only reads it.

/*
====================
CGenericCar::fn_8007B8F0
====================
*/
// 0x8007B8F0
void CGenericCar::fn_8007B8F0( int slot ) {
    if ( slot < 0 ) {
        fn_80019F74( this, slot );
        return;
    }

    unk_HashTable* table = field_0x14C;
    unk_HashEntry* entry = ( table == slots[slot].owner ) ? slots[slot].entry : NULL;
    if ( entry == NULL ) {
        entry = fn_800A2A18( table, slots[slot].name, slots[slot].hash );
        if ( entry == NULL ) {
            entry = fn_800A2AB8( table, slots[slot].name, 3 );
        }
        slots[slot].entry = entry;
        slots[slot].owner = table;
    }
    entry->value = lbl_8052721C;
}
