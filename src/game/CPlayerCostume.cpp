// CPlayerCostume: class name from the registration strings lbl_803E646C and lbl_803E647C.
// The inline factory fn_80073F30 and the destructor thunk follow the static initialiser in .text.

#include <game/CPlayerCostume.h>

u32 lbl_80529580 = fn_801A7A84( "CPlayerCostume" );
unk_801A8F6C lbl_80529584( "CPlayerCostume", CPlayerCostume::fn_80073F30, sizeof( CPlayerCostume ) );

/*
====================
CPlayerCostume::~CPlayerCostume

Stops being the player's costume.
====================
*/
// 0x80073E3C
CPlayerCostume::~CPlayerCostume() {
    unk_80491B38& record = GetPlayerRecord();
    if ( record.field_0x18 == this ) {
        record.field_0x18 = NULL;
    }
}
