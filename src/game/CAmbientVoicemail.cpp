#include <game/CAmbientVoicemail.h>
#include <game/DisplayHeight.h>

#define SAFE_DELETE( p )    if ( p ) { delete ( p ); ( p ) = 0; }

/*
====================
CAmbientVoicemail::~CAmbientVoicemail
====================
*/
// 0x800C2FF0
CAmbientVoicemail::~CAmbientVoicemail() {
    SAFE_DELETE( field_0xE8 );
    SAFE_DELETE( field_0xEC );
    SAFE_DELETE( field_0xF0 );
    SAFE_DELETE( field_0xF4 );
}

/*
====================
CAmbientVoicemail::fn_8009CB1C
====================
*/
// 0x800C30CC
void CAmbientVoicemail::fn_8009CB1C( unk_Msg& msg ) {
    CAmbientInterferencePoint::fn_8009CB1C( msg );
}

/*
====================
CAmbientVoicemail::fn_800C2790

When the base accepts, hands the voicemail to the inbox.
====================
*/
// 0x800C30D0
int CAmbientVoicemail::fn_800C2790() {
    int result = CAmbientInterferencePoint::fn_800C2790();
    if ( result ) {
        lbl_80529D20->field_0x9C.fn_800F5AE8( unk_MessageInfo( field_0xE8, field_0xEC, field_0xF0, field_0xF4, field_0xF8 ) );
        fn_800C2A0C( field_0xF0, true );
    }
    return result;
}

/*
====================
CAmbientVoicemail::fn_801A73A8

Reads the voicemail's four strings and its value from the property stream.
====================
*/
// 0x800C315C
void CAmbientVoicemail::fn_801A73A8( const unk_AttributePacket& packet ) {
    CAmbientInterferencePoint::fn_801A73A8( packet );
    for ( unk_AttributeCommandIterator it( packet, lbl_80529A60 ); !it.IsFinished(); ++it ) {
        switch ( it->id ) {
            case CMD_SetFieldE8:
                field_0xE8 = fn_801B67FC( it->GetString(), 0 );
                break;
            case CMD_SetFieldEC:
                field_0xEC = fn_801B67FC( it->GetString(), 0 );
                break;
            case CMD_SetFieldF0:
                field_0xF0 = fn_801B67FC( it->GetString(), 0 );
                break;
            case CMD_SetFieldF4:
                field_0xF4 = fn_801B67FC( it->GetString(), 0 );
                break;
            case CMD_SetFieldF8:
                field_0xF8 = it->GetU32();
                break;
        }
    }
    fn_800C2A0C( field_0xF0, false );
}

u32             lbl_80529A60 = fn_801A7A84( "CAmbientVoicemail" );
unk_801A8F6C    lbl_80529A64( "CAmbientVoicemail", CAmbientVoicemail::fn_800C32FC, sizeof( CAmbientVoicemail ) );
