#ifndef GAME_UNK_80043950_H
#define GAME_UNK_80043950_H

#include <revolution/types.h>
#include <game/unk_80491B38.h>
#include <game/unk_8000F80C.h>
#include <game/unk_80055174.h>
struct RwV3d;
class unk_8003C454;

// Globals this unit reads; defined in units not yet reconstructed. Names are the retail labels.
extern u8 lbl_80529F04;
extern void* lbl_80529F00;

/*
 * TODO: class NVec3: base class, members and vtable are not recovered yet. Declared from
 * the mangled names of the unit's functions (return types are unknown and shown as void).
 */
class NVec3 {
public:
    void SquareDistance( const RwV3d& ) const;
    void Length() const;
    void Normalize();
    // TODO: __apl__5NVec3FRC5NVec3
    void operator*( float ) const;
    void SquaredLength() const;
    void operator!=( const NVec3& ) const;
    // TODO: __amu__5NVec3Ff
};

/*
 * A name with its hash, set by fn_80043D04 (struct and field names invented). The third word is
 * zeroed by the setter; its meaning is not recovered yet.
 */
struct unk_NameHash {
    const char* name;       // 0x0
    u32         hash;       // 0x4: case-folded hash of name, or the caller's hash when non-zero
    void*       field_0x8;  // 0x8
};

void operator+( const RwV3d&, const RwV3d& );

extern "C" {
void fn_80043950();
void fn_80043D00();
void fn_80043D04( unk_NameHash* self, const char* name, u32 hash );
void fn_80043D6C();
void fn_80043E44();
void fn_8004414C();
void* fn_80044184();
void fn_800441A0( unk_8003C454* self );
void fn_800441A4();
void fn_80044248();
unk_80491B38* fn_80044338();
void fn_80044344();
void fn_80044460();
void fn_800448B0();
void fn_80044948();
void fn_800449E0();
void fn_80044A78();
void fn_80044B10();
void fn_80044BA8();
void fn_80044ED0();
void fn_80044EEC();
void fn_80044EF4();
void fn_80044EFC();
void fn_80045054();
void fn_80045070();
void fn_80045688();
void fn_80045740();
void fn_800457C4();
void fn_800457E0();
void fn_80045858();
void fn_80045A68();
void fn_80045A90();
void fn_80045B40();
void fn_80045DA4();
void fn_80045DAC();
void fn_80045DC0();
void fn_80045DE8();
void fn_80046250();
void fn_8004641C();
void fn_80046510();
void fn_8004687C();
void fn_80046908();
void fn_80047454();
void fn_8004748C();
void fn_80047494();
void fn_80047520();
void fn_80047534();
void fn_800475D0();
void fn_80047670();
void fn_80047694();
void fn_80047728();
void fn_8004788C();
void fn_800478D8();
void fn_80047FFC();
void fn_8004838C();
void fn_80048414();
void fn_8004844C();
void fn_80048450();
void fn_80048684();
void fn_800489C8();
void fn_80048A04();
void fn_800490E8();
void fn_8004913C();
void fn_800499AC();
void fn_800499F0();
void fn_80049A68();
void fn_80049AF0();
void fn_80049E84();
void fn_80049FBC();
void fn_8004A378();
void fn_8004A3F8();
void fn_8004A498();
void fn_8004A510();
void fn_8004A8BC();
void fn_8004B2F0();
void fn_8004B704();
void fn_8004B818();
void fn_8004B938();
void fn_8004B944();
void fn_8004B950();
void fn_8004BB04();
void fn_8004BC94();
void fn_8004BE2C();
void fn_8004BFE4();
void fn_8004C094();
void fn_8004C25C();
void fn_8004C3F4();
void fn_8004C3FC();
void fn_8004C404();
void fn_8004C45C();
void fn_8004C610();
void fn_8004C670();
void fn_8004C7B4();
void fn_8004C804();
void fn_8004C8B8();
void fn_8004C960();
void fn_8004D0BC();
void fn_8004DA3C();
void fn_8004DFFC();
void fn_8004E0C0();
void fn_8004E2C0();
void fn_8004E35C();
void fn_8004E7C8();
void fn_8004E7DC();
void fn_8004F060();
void fn_8004F1C8();
void fn_8004F248();
void fn_8004F2C8();
void fn_8004F38C();
void fn_8004FAD4();
void fn_8004FD38();
void fn_8004FEE0();
void fn_800501EC();
void fn_80050370();
void fn_80050378();
void fn_8005042C();
void fn_80050464();
void fn_8005081C();
void fn_80050938();
void fn_80050998();
void fn_80051090();
void fn_800511AC();
void fn_80051238();
void fn_80051C70();
void fn_80051D04();
void fn_80051F2C();
void fn_80052018();
void fn_80052070();
void fn_80052088();
void fn_800520A0();
void fn_80052160();
void fn_8005222C();
void fn_80052314();
void fn_80052384();
void fn_8005259C();
void fn_800527BC();
void fn_80052824();
void fn_800529EC();
void fn_80052C04();
}

#endif
