// cVHSConfig: class name from the registration strings lbl_803E8838 and lbl_803E8844.
// The inline factory fn_800913F8 and the two thunks follow the static initialiser in .text.

#include <game/cVHSConfig.h>

/*
====================
cVHSConfig::~cVHSConfig
====================
*/
// 0x80091108
cVHSConfig::~cVHSConfig() {
    fn_801A9E00( onEvent );
    fn_801A9940( onEvent );
    fn_801A9E00( offEvent );
    fn_801A9940( offEvent );
    cVHSConfig* cur = lbl_80529720;
    lbl_80529720 = NULL;
    while ( cur != NULL ) {
        cVHSConfig* item = cur;
        cur = cur->next;
        if ( item != this ) {
            item->next = lbl_80529720;
            lbl_80529720 = item;
        }
    }
}

/*
====================
fn_800911F0

Returns the largest value of the active configs, or 0.0 when none is active. Name invented.
====================
*/
// 0x800911F0
float fn_800911F0() {
    float largest = 0.0f;
    for ( cVHSConfig* cur = lbl_80529720; cur != NULL; cur = cur->next ) {
        if ( cur->active && cur->value > largest ) {
            largest = cur->value;
        }
    }
    return largest;
}

/*
====================
cVHSConfig::fn_8009CB1C

Activates the config on its on message and deactivates it on its off message.
====================
*/
// 0x80091228
void cVHSConfig::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == onEvent.record ) {
        active = true;
    }
    if ( msg.record == offEvent.record ) {
        active = false;
    }
}

/*
====================
cVHSConfig::fn_801A73A8

Takes the active flag, the value and the on and off messages from the attribute commands.
====================
*/
// 0x80091290
void cVHSConfig::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529718 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetActive:
            if ( it->GetU32() != 0 ) {
                active = true;
            } else {
                active = false;
            }
            break;
        case CMD_SetOnMsg:
            fn_801AA218( onEvent, it->GetString(), NULL );
            break;
        case CMD_SetOffMsg:
            fn_801AA218( offEvent, it->GetString(), NULL );
            break;
        case CMD_SetValue:
            value = it->GetFloat();
            break;
        }
        ++it;
    }
}

u32 lbl_80529718 = fn_801A7A84( "cVHSConfig" );
unk_801A8F6C lbl_8052971C( "cVHSConfig", cVHSConfig::fn_800913F8, sizeof( cVHSConfig ) );
cVHSConfig* lbl_80529720;
