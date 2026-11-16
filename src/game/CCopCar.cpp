// unit name unconfirmed: the class name is the registration string "CCopCar"; the file name is
// invented. The inline overrides of CGenericCar follow CCopCar's own functions, and the inline
// factory fn_80077B8C and the thunks follow the static initialiser in .text.

#include <game/CCopCar.h>
#include <game/DisplayHeight.h>

u32 lbl_80529598 = fn_801A7A84( "CCopCar" );
unk_801A8F6C lbl_8052959C( "CCopCar", CCopCar::fn_80077B8C, sizeof( CCopCar ) );

/*
====================
CCopCar::~CCopCar
====================
*/
// 0x80077A8C
CCopCar::~CCopCar() {}

/*
====================
CCopCar::fn_801A73A8
====================
*/
// 0x80077AE4
void CCopCar::fn_801A73A8( const unk_AttributePacket& packet ) {
    CGenericCar::fn_801A73A8( packet );
}

/*
====================
CCopCar::fn_800180E4
====================
*/
// 0x80077AE8
void CCopCar::fn_800180E4() {
    CGenericCar::fn_800180E4();
}

/*
====================
CCopCar::fn_80019F88
====================
*/
// 0x80077AEC
void CCopCar::fn_80019F88( float dt ) {
    CGenericCar::fn_80019F88( dt );
}

/*
====================
CCopCar::fn_8009CB1C
====================
*/
// 0x80077AF0
void CCopCar::fn_8009CB1C( unk_Msg& msg ) {
    CGenericCar::fn_8009CB1C( msg );
}
