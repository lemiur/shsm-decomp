// unk_800C8598: unit name unconfirmed (no string names the class). It precedes CColdTrigger's
// unit: its vtable and its flag lbl_80527508 end their own 8-byte aligned .data and .sdata blocks.

#include <game/unk_800C8598.h>

bool lbl_80527508 = true;

/*
====================
unk_800C8598::unk_800C8598
====================
*/
// 0x800C8598
unk_800C8598::unk_800C8598( unk_801A92A8* forwardMsg ) : forwardMsg( forwardMsg ) {
    behaviour = NULL;
    enabled = true;
    fn_801A9DE8( lbl_8052A780 );
}

/*
====================
unk_800C8598::~unk_800C8598
====================
*/
// 0x800C8600
unk_800C8598::~unk_800C8598() {
    fn_801A9E00( lbl_8052A780 );
}

/*
====================
unk_800C8598::fn_8009CB1C

Forwards lbl_8052A780 with the behaviour's position.
====================
*/
// 0x800C8670
void unk_800C8598::fn_8009CB1C( unk_Msg& msg ) {
    if ( behaviour != NULL && enabled && lbl_80527508 && msg.record == lbl_8052A780.record
        && forwardMsg->registrations > 0 && behaviour->mZone != NULL ) {
        const unk_RwV3d* pos = behaviour->fn_8009CD08();
        if ( pos != NULL ) {
            unk_ColdMsgData data;
            data.sender = this;
            data.pos = *pos;
            data.zone = behaviour->mZone;
            data.type = 2;
            PostMsg( *forwardMsg, &data );
        }
    }
}
