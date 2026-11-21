// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80055174.h>
#include <game/unk_RespotManager.h>

/*
 * Callees not yet reconstructed under C++ names (placeholders by address). fn_80030B68 is a
 * zone-change routine of the respot manager (lbl_80529260), its manager argument unused; fn_80042F18
 * is unk_8003C454's override of the same slot, which this override calls first.
 */
extern "C" {
    void    fn_80030B68( unk_RespotManager* manager, CZoneChangeInfo* change );
    void    fn_80042F18( unk_8003C454* self, CZoneChangeInfo* change );
}

/*
====================
unk_80055174::fn_801A7BFC
====================
*/
// 0x80055310
void unk_80055174::fn_801A7BFC( CZoneChangeInfo* change ) {
    fn_80030B68( lbl_80529260, change );
    fn_80042F18( this, change );
}
