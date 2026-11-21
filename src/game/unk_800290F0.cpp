// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/CZoneChangeInfo.h>
#include <game/unk_RespotManager.h>

/*
====================
unk_ZoneHandle::~unk_ZoneHandle
====================
*/
// 0x800290F0
unk_ZoneHandle::~unk_ZoneHandle() {
    fn_80031E18( lbl_80529260, this );
    if ( this != NULL && zone != NULL ) {
        fn_80028180( zone );
    }
}
