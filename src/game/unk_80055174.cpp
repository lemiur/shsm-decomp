// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80055174.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803E3BD8 size 0x298 lbl_803E3BD8
//   .sdata2 0x8052BB38 size 0x4 lbl_8052BB38
//   .sdata2 0x8052BB3C size 0x4 lbl_8052BB3C
//   .sdata2 0x8052BB40 size 0x4 lbl_8052BB40
//   .sdata2 0x8052BB44 size 0x4 lbl_8052BB44

/*
====================
unk_80055174::unk_80055174

Not matched: the one multiply that makes the half angle differs in register/schedule only (retail
keeps the radians in f1 and the constant in f0; the compiled order is swapped).
====================
*/
// 0x80055174
unk_80055174::unk_80055174( const unk_AttributePacket& packet ) : unk_8003C454( packet ), CPointOfInterest( packet ) {
    field_0x778 = 0xFFFF;
    field_0x75C.x = field_0x75C.y = field_0x75C.z = 0.0f;
    field_0x77C = 3.1415927f * field_0x69C / 180.0f;
    field_0x780 = 0.5f * field_0x77C;
    field_0x785 = true;
    matrix = RwFrameGetMatrix( CBaseBehaviour::field_0x30 );
}

/*
====================
unk_80055174::~unk_80055174
====================
*/
// 0x8005524C
unk_80055174::~unk_80055174() {
}

/*
====================
unk_80055174::fn_800552B4
====================
*/
// 0x800552B4
void unk_80055174::fn_800552B4() {
    fn_8009CD84( &field_0x750 );
    fn_8009CCE8( &field_0x744 );
    field_0x768.x = field_0x3CC.x;
    field_0x768.y = field_0x3CC.y;
    field_0x768.z = field_0x3CC.z;
    field_0x774 = field_0x3E4;
}
