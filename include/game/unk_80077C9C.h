#ifndef GAME_UNK_80077C9C_H
#define GAME_UNK_80077C9C_H

#include <revolution/types.h>

/* The node a lookup returns (fn_800A2A18). Name invented; members after the one read here. */
struct unk_LookupResultNode {
    u8              field_0x0[0x10];
    u32             field_0x10;     // the value of the lookup
};

/* One lookup record, reached through unk_LookupClient's record table. Name invented. */
struct unk_LookupMissRecord {
    u8              field_0x0[0x20];
    s32             field_0x20;     // set to 1 when the lookup finds no value
};

/* The cached result of one lookup: the table and parameter it is made from, and the last answer. Name invented. */
struct unk_Cache {
    void*           table;          // 0x00, the table passed to fn_800A2A18
    s32             param;          // 0x04, the parameter passed to fn_800A2A18
    unk_LookupResultNode*  node;           // 0x08, the last answer, null when none
    u32             key;            // 0x0C, the key the answer was made for
};

/*
 * The object the lookup functions read: a key, one cache per lookup and the record table.
 * Only the fields the functions read are named; the rest are gaps. Name invented.
 */
struct unk_LookupClient {
    u8              field_0x0[0x14C];
    u32             field_0x14C;    // the key of the current lookup
    u8              field_0x150[0x820 - 0x150];
    unk_Cache       field_0x820;    // lookup 0
    u8              field_0x830[0x900 - 0x830];
    unk_Cache       field_0x900;    // lookup 1
    u8              field_0x910[0x9E0 - 0x910];
    unk_Cache       field_0x9E0;    // lookup 18
    u8              field_0x9F0[0xAC0 - 0x9F0];
    unk_Cache       field_0xAC0;    // lookup 19
    u8              field_0xAD0[0xBA0 - 0xAD0];
    unk_Cache       field_0xBA0;    // lookup 20
    u8              field_0xBB0[0xC80 - 0xBB0];
    unk_Cache       field_0xC80;    // lookup 21
    u8              field_0xC90[0xD60 - 0xC90];
    unk_Cache       field_0xD60;    // lookup 22
    u8              field_0xD70[0xE40 - 0xD70];
    unk_Cache       field_0xE40;    // lookup 6
    u8              field_0xE50[0xF20 - 0xE50];
    unk_Cache       field_0xF20;    // lookup 7
    u8              field_0xF30[0x1000 - 0xF30];
    unk_Cache       field_0x1000;   // lookup 8
    u8              field_0x1010[0x10E0 - 0x1010];
    unk_Cache       field_0x10E0;   // lookup 9
    u8              field_0x10F0[0x11C0 - 0x10F0];
    unk_Cache       field_0x11C0;   // lookup 2
    u8              field_0x11D0[0x12A0 - 0x11D0];
    unk_Cache       field_0x12A0;   // lookup 3
    u8              field_0x12B0[0x1380 - 0x12B0];
    unk_Cache       field_0x1380;   // lookup 4
    u8              field_0x1390[0x1460 - 0x1390];
    unk_Cache       field_0x1460;   // lookup 5
    u8              field_0x1470[0x1540 - 0x1470];
    unk_Cache       field_0x1540;   // lookup 10
    u8              field_0x1550[0x1620 - 0x1550];
    unk_Cache       field_0x1620;   // lookup 11
    u8              field_0x1630[0x1700 - 0x1630];
    unk_Cache       field_0x1700;   // lookup 12
    u8              field_0x1710[0x17E0 - 0x1710];
    unk_Cache       field_0x17E0;   // lookup 13
    u8              field_0x17F0[0x18C0 - 0x17F0];
    unk_Cache       field_0x18C0;   // lookup 14
    u8              field_0x18D0[0x19A0 - 0x18D0];
    unk_Cache       field_0x19A0;   // lookup 15
    u8              field_0x19B0[0x1A80 - 0x19B0];
    unk_Cache       field_0x1A80;   // lookup 16
    u8              field_0x1A90[0x1B60 - 0x1A90];
    unk_Cache       field_0x1B60;   // lookup 17
    u8              field_0x1B70[0x1C40 - 0x1B70];
    unk_Cache       field_0x1C40;   // lookup 23
    u8              field_0x1C50[0x1D20 - 0x1C50];
    unk_Cache       field_0x1D20;   // lookup 24
    u8              field_0x1D30[0x1E00 - 0x1D30];
    unk_Cache       field_0x1E00;   // lookup 25
    u8              field_0x1E10[0x1EE0 - 0x1E10];
    unk_Cache       field_0x1EE0;   // lookup 26
    u8              field_0x1EF0[0x1FC0 - 0x1EF0];
    unk_Cache       field_0x1FC0;   // lookup 27
    u8              field_0x1FD0[0x2018 - 0x1FD0];
    unk_LookupMissRecord*     field_0x2018;   // record of lookup 0
    unk_LookupMissRecord*     field_0x201C;   // record of lookup 1
    unk_LookupMissRecord*     field_0x2020;   // record of lookup 18
    unk_LookupMissRecord*     field_0x2024;   // record of lookup 19
    unk_LookupMissRecord*     field_0x2028;   // record of lookup 20
    unk_LookupMissRecord*     field_0x202C;   // record of lookup 21
    unk_LookupMissRecord*     field_0x2030;   // record of lookup 22
    unk_LookupMissRecord*     field_0x2034;   // record of lookup 6
    unk_LookupMissRecord*     field_0x2038;   // record of lookup 7
    unk_LookupMissRecord*     field_0x203C;   // record of lookup 8
    unk_LookupMissRecord*     field_0x2040;   // record of lookup 9
    unk_LookupMissRecord*     field_0x2044;   // record of lookup 2
    unk_LookupMissRecord*     field_0x2048;   // record of lookup 3
    unk_LookupMissRecord*     field_0x204C;   // record of lookup 4
    unk_LookupMissRecord*     field_0x2050;   // record of lookup 5
    unk_LookupMissRecord*     field_0x2054;   // record of lookup 10
    unk_LookupMissRecord*     field_0x2058;   // record of lookup 11
    unk_LookupMissRecord*     field_0x205C;   // record of lookup 12
    unk_LookupMissRecord*     field_0x2060;   // record of lookup 13
    unk_LookupMissRecord*     field_0x2064;   // record of lookup 14
    unk_LookupMissRecord*     field_0x2068;   // record of lookup 15
    unk_LookupMissRecord*     field_0x206C;   // record of lookup 16
    unk_LookupMissRecord*     field_0x2070;   // record of lookup 17
    unk_LookupMissRecord*     field_0x2074;   // record of lookup 23
    unk_LookupMissRecord*     field_0x2078;   // record of lookup 24
    unk_LookupMissRecord*     field_0x207C;   // record of lookup 25
    unk_LookupMissRecord*     field_0x2080;   // record of lookup 26
    unk_LookupMissRecord*     field_0x2084;   // record of lookup 27
};

extern "C" {
/* Defined in the unit of 0x800A292C, not yet reconstructed: the lookup itself. Name invented. */
unk_LookupResultNode*  fn_800A2A18( u32 key, void* table, s32 param );

void fn_80077C9C( int, unk_LookupClient* target );
void fn_80077D40();
void fn_80077D44( int, unk_LookupClient* target );
void fn_80077DE8();
void fn_80077DEC( int, unk_LookupClient* target );
void fn_80077E90();
void fn_80077E94( int, unk_LookupClient* target );
void fn_80077F38();
void fn_80077F3C( int, unk_LookupClient* target );
void fn_80077FE0();
void fn_80077FE4( int, unk_LookupClient* target );
void fn_80078088();
void fn_8007808C( int, unk_LookupClient* target );
void fn_80078130();
void fn_80078134( int, unk_LookupClient* target );
void fn_800781D8();
void fn_800781DC( int, unk_LookupClient* target );
void fn_80078280();
void fn_80078284( int, unk_LookupClient* target );
void fn_80078328();
void fn_8007832C( int, unk_LookupClient* target );
void fn_800783D0();
void fn_800783D4( int, unk_LookupClient* target );
void fn_80078478();
void fn_8007847C( int, unk_LookupClient* target );
void fn_80078520();
void fn_80078524( int, unk_LookupClient* target );
void fn_800785C8();
void fn_800785CC( int, unk_LookupClient* target );
void fn_80078670();
void fn_80078674( int, unk_LookupClient* target );
void fn_80078718();
void fn_8007871C( int, unk_LookupClient* target );
void fn_800787C0();
void fn_800787C4( int, unk_LookupClient* target );
void fn_80078868();
void fn_8007886C( int, unk_LookupClient* target );
void fn_80078910();
void fn_80078914( int, unk_LookupClient* target );
void fn_800789B8();
void fn_800789BC( int, unk_LookupClient* target );
void fn_80078A60();
void fn_80078A64( int, unk_LookupClient* target );
void fn_80078B08();
void fn_80078B0C( int, unk_LookupClient* target );
void fn_80078BB0();
void fn_80078BB4( int, unk_LookupClient* target );
void fn_80078C58();
void fn_80078C5C( int, unk_LookupClient* target );
void fn_80078D00();
void fn_80078D04( int, unk_LookupClient* target );
void fn_80078DA8();
void fn_80078DAC( int, unk_LookupClient* target );
void fn_80078E50();
void fn_80078E54( int, unk_LookupClient* target );
void fn_80078EF8();
}

#endif
