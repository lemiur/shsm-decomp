// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80077C9C.h>

/*
====================
fn_80077C9C
====================
*/
// Lookup 0: the first argument is unused (r3); the object is the second argument (r4).
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80077C9C
extern "C" void fn_80077C9C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2018;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x820.key ) ? target->field_0x820.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x820.table, target->field_0x820.param );
        target->field_0x820.node = node;
        target->field_0x820.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80077D40
====================
*/
// 0x80077D40
extern "C" void fn_80077D40() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80077D44
====================
*/
// Lookup: the same as fn_80077C9C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80077D44
extern "C" void fn_80077D44( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x201C;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x900.key ) ? target->field_0x900.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x900.table, target->field_0x900.param );
        target->field_0x900.node = node;
        target->field_0x900.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80077DE8
====================
*/
// 0x80077DE8
extern "C" void fn_80077DE8() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80077DEC
====================
*/
// Lookup: the same as fn_80077C9C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80077DEC
extern "C" void fn_80077DEC( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2044;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x11C0.key ) ? target->field_0x11C0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x11C0.table, target->field_0x11C0.param );
        target->field_0x11C0.node = node;
        target->field_0x11C0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80077E90
====================
*/
// 0x80077E90
extern "C" void fn_80077E90() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80077E94
====================
*/
// Lookup: the same as fn_80077C9C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80077E94
extern "C" void fn_80077E94( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2048;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x12A0.key ) ? target->field_0x12A0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x12A0.table, target->field_0x12A0.param );
        target->field_0x12A0.node = node;
        target->field_0x12A0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80077F38
====================
*/
// 0x80077F38
extern "C" void fn_80077F38() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80077F3C
====================
*/
// Lookup: the same as fn_80077C9C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80077F3C
extern "C" void fn_80077F3C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x204C;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1380.key ) ? target->field_0x1380.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1380.table, target->field_0x1380.param );
        target->field_0x1380.node = node;
        target->field_0x1380.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80077FE0
====================
*/
// 0x80077FE0
extern "C" void fn_80077FE0() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80077FE4
====================
*/
// Lookup: the same as fn_80077C9C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80077FE4
extern "C" void fn_80077FE4( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2050;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1460.key ) ? target->field_0x1460.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1460.table, target->field_0x1460.param );
        target->field_0x1460.node = node;
        target->field_0x1460.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078088
====================
*/
// 0x80078088
extern "C" void fn_80078088() {
    // Empty in retail: a no-op.
}

/*
====================
fn_8007808C
====================
*/
// Lookup 6: the first argument is unused (r3); the object is the second argument (r4).
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x8007808C
extern "C" void fn_8007808C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2034;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0xE40.key ) ? target->field_0xE40.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0xE40.table, target->field_0xE40.param );
        target->field_0xE40.node = node;
        target->field_0xE40.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078130
====================
*/
// 0x80078130
extern "C" void fn_80078130() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078134
====================
*/
// Lookup 7: the same as fn_8007808C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078134
extern "C" void fn_80078134( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2038;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0xF20.key ) ? target->field_0xF20.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0xF20.table, target->field_0xF20.param );
        target->field_0xF20.node = node;
        target->field_0xF20.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_800781D8
====================
*/
// 0x800781D8
extern "C" void fn_800781D8() {
    // Empty in retail: a no-op.
}

/*
====================
fn_800781DC
====================
*/
// Lookup 8: the same as fn_8007808C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x800781DC
extern "C" void fn_800781DC( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x203C;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1000.key ) ? target->field_0x1000.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1000.table, target->field_0x1000.param );
        target->field_0x1000.node = node;
        target->field_0x1000.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078280
====================
*/
// 0x80078280
extern "C" void fn_80078280() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078284
====================
*/
// Lookup 9: the same as fn_8007808C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078284
extern "C" void fn_80078284( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2040;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x10E0.key ) ? target->field_0x10E0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x10E0.table, target->field_0x10E0.param );
        target->field_0x10E0.node = node;
        target->field_0x10E0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078328
====================
*/
// 0x80078328
extern "C" void fn_80078328() {
    // Empty in retail: a no-op.
}

/*
====================
fn_8007832C
====================
*/
// Lookup 10: the same as fn_8007808C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x8007832C
extern "C" void fn_8007832C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2054;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1540.key ) ? target->field_0x1540.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1540.table, target->field_0x1540.param );
        target->field_0x1540.node = node;
        target->field_0x1540.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_800783D0
====================
*/
// 0x800783D0
extern "C" void fn_800783D0() {
    // Empty in retail: a no-op.
}

/*
====================
fn_800783D4
====================
*/
// Lookup 11: the same as fn_8007808C with its own record and cache fields.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x800783D4
extern "C" void fn_800783D4( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2058;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1620.key ) ? target->field_0x1620.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1620.table, target->field_0x1620.param );
        target->field_0x1620.node = node;
        target->field_0x1620.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078478
====================
*/
// 0x80078478
extern "C" void fn_80078478() {
    // Empty in retail: a no-op.
}

/*
====================
fn_8007847C
====================
*/
// Lookup 12: the first argument is unused (r3); the object is the second argument (r4).
// 
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x8007847C
extern "C" void fn_8007847C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x205C;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1700.key ) ? target->field_0x1700.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1700.table, target->field_0x1700.param );
        target->field_0x1700.node = node;
        target->field_0x1700.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078520
====================
*/
// 0x80078520
extern "C" void fn_80078520() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078524
====================
*/
// Lookup 13: the first argument is unused (r3); the object is the second argument (r4).
// 
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078524
extern "C" void fn_80078524( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2060;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x17E0.key ) ? target->field_0x17E0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x17E0.table, target->field_0x17E0.param );
        target->field_0x17E0.node = node;
        target->field_0x17E0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_800785C8
====================
*/
// 0x800785C8
extern "C" void fn_800785C8() {
    // Empty in retail: a no-op.
}

/*
====================
fn_800785CC
====================
*/
// Lookup 14: the first argument is unused (r3); the object is the second argument (r4).
// 
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x800785CC
extern "C" void fn_800785CC( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2064;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x18C0.key ) ? target->field_0x18C0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x18C0.table, target->field_0x18C0.param );
        target->field_0x18C0.node = node;
        target->field_0x18C0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078670
====================
*/
// 0x80078670
extern "C" void fn_80078670() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078674
====================
*/
// Lookup 15: the first argument is unused (r3); the object is the second argument (r4).
// 
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078674
extern "C" void fn_80078674( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2068;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x19A0.key ) ? target->field_0x19A0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x19A0.table, target->field_0x19A0.param );
        target->field_0x19A0.node = node;
        target->field_0x19A0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078718
====================
*/
// 0x80078718
extern "C" void fn_80078718() {
    // Empty in retail: a no-op.
}

/*
====================
fn_8007871C
====================
*/
// Lookup 16: the first argument is unused (r3); the object is the second argument (r4).
// 
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x8007871C
extern "C" void fn_8007871C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x206C;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1A80.key ) ? target->field_0x1A80.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1A80.table, target->field_0x1A80.param );
        target->field_0x1A80.node = node;
        target->field_0x1A80.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_800787C0
====================
*/
// 0x800787C0
extern "C" void fn_800787C0() {
    // Empty in retail: a no-op.
}

/*
====================
fn_800787C4
====================
*/
// Lookup 17: the first argument is unused (r3); the object is the second argument (r4).
// 
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in the register order of retail (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x800787C4
extern "C" void fn_800787C4( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2070;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1B60.key ) ? target->field_0x1B60.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1B60.table, target->field_0x1B60.param );
        target->field_0x1B60.node = node;
        target->field_0x1B60.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078868
====================
*/
// 0x80078868
extern "C" void fn_80078868() {
    // Empty in retail: a no-op.
}

/*
====================
fn_8007886C
====================
*/
// Lookup 18: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x8007886C
extern "C" void fn_8007886C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2020;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x9E0.key ) ? target->field_0x9E0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x9E0.table, target->field_0x9E0.param );
        target->field_0x9E0.node = node;
        target->field_0x9E0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078910
====================
*/
// 0x80078910
extern "C" void fn_80078910() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078914
====================
*/
// Lookup 19: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078914
extern "C" void fn_80078914( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2024;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0xAC0.key ) ? target->field_0xAC0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0xAC0.table, target->field_0xAC0.param );
        target->field_0xAC0.node = node;
        target->field_0xAC0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_800789B8
====================
*/
// 0x800789B8
extern "C" void fn_800789B8() {
    // Empty in retail: a no-op.
}

/*
====================
fn_800789BC
====================
*/
// Lookup 20: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x800789BC
extern "C" void fn_800789BC( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2028;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0xBA0.key ) ? target->field_0xBA0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0xBA0.table, target->field_0xBA0.param );
        target->field_0xBA0.node = node;
        target->field_0xBA0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078A60
====================
*/
// 0x80078A60
extern "C" void fn_80078A60() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078A64
====================
*/
// Lookup 21: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078A64
extern "C" void fn_80078A64( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x202C;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0xC80.key ) ? target->field_0xC80.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0xC80.table, target->field_0xC80.param );
        target->field_0xC80.node = node;
        target->field_0xC80.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078B08
====================
*/
// 0x80078B08
extern "C" void fn_80078B08() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078B0C
====================
*/
// Lookup 22: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078B0C
extern "C" void fn_80078B0C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2030;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0xD60.key ) ? target->field_0xD60.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0xD60.table, target->field_0xD60.param );
        target->field_0xD60.node = node;
        target->field_0xD60.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078BB0
====================
*/
// 0x80078BB0
extern "C" void fn_80078BB0() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078BB4
====================
*/
// Lookup 23: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078BB4
extern "C" void fn_80078BB4( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2074;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1C40.key ) ? target->field_0x1C40.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1C40.table, target->field_0x1C40.param );
        target->field_0x1C40.node = node;
        target->field_0x1C40.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078C58
====================
*/
// 0x80078C58
extern "C" void fn_80078C58() {
    // Empty in retail: a no-op.
}

/*
====================
fn_80078C5C
====================
*/
// Lookup 24: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078C5C
extern "C" void fn_80078C5C( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2078;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1D20.key ) ? target->field_0x1D20.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1D20.table, target->field_0x1D20.param );
        target->field_0x1D20.node = node;
        target->field_0x1D20.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078D00
====================
*/
// 0x80078D00
extern "C" void fn_80078D00() {
    // TODO
}

/*
====================
fn_80078D04
====================
*/
// Lookup 25: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078D04
extern "C" void fn_80078D04( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x207C;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1E00.key ) ? target->field_0x1E00.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1E00.table, target->field_0x1E00.param );
        target->field_0x1E00.node = node;
        target->field_0x1E00.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078DA8
====================
*/
// 0x80078DA8
extern "C" void fn_80078DA8() {
    // TODO
}

/*
====================
fn_80078DAC
====================
*/
// Lookup 26: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078DAC
extern "C" void fn_80078DAC( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2080;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1EE0.key ) ? target->field_0x1EE0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1EE0.table, target->field_0x1EE0.param );
        target->field_0x1EE0.node = node;
        target->field_0x1EE0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078E50
====================
*/
// 0x80078E50
extern "C" void fn_80078E50() {
    // TODO
}

/*
====================
fn_80078E54
====================
*/
// Lookup 27: the first argument is unused (r3); the object is the second argument (r4). The record's flag is set when the answer's value, or the key when there is no answer, is zero.
// RULE-EXCEPTION: R30, the key and value locals are declared before the record in retail's register order (r31 key, r30 record); the key is loaded after the record check, as retail does.
// 0x80078E54
extern "C" void fn_80078E54( int, unk_LookupClient* target ) {
    if ( target == NULL ) {
        return;
    }
    u32 key;
    u32 value;
    unk_LookupMissRecord* record = target->field_0x2084;
    if ( record == NULL ) {
        return;
    }
    key = target->field_0x14C;
    value = key;
    unk_LookupResultNode* node = ( key == target->field_0x1FC0.key ) ? target->field_0x1FC0.node : NULL;
    if ( node == NULL ) {
        node = fn_800A2A18( key, target->field_0x1FC0.table, target->field_0x1FC0.param );
        target->field_0x1FC0.node = node;
        target->field_0x1FC0.key = key;
    }
    if ( node != NULL ) {
        value = node->field_0x10;
    }
    if ( value == 0 ) {
        record->field_0x20 = 1;
    }
}

/*
====================
fn_80078EF8
====================
*/
// 0x80078EF8
extern "C" void fn_80078EF8() {
    // TODO
}
