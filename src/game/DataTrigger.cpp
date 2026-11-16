// DataTrigger: class name from the registration strings lbl_803EC130 and lbl_803EC13C.
// The inline factory fn_800C9378 and the two thunks follow the static initialiser in .text.

#include <game/DataTrigger.h>
#include <game/Log.h>

const char* lbl_80527518 = "SaveData";

/*
====================
DataTrigger::~DataTrigger
====================
*/
// 0x800C90DC
DataTrigger::~DataTrigger() {
    delete[] key;
    delete[] value;
    fn_801A9E00( msg );
    fn_801A9940( msg );
}

/*
====================
DataTrigger::fn_801A73A8

Takes the key, the value and the name of the message from the attribute packet.
====================
*/
// 0x800C9184
void DataTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529AE8 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetKey:
            key = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetValue:
            value = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetMsg:
            fn_801AA218( msg, it->GetString(), 0 );
            break;
        }
        ++it;
    }
    if ( value != NULL ) {
        fn_802061DC( value );
    }
}

/*
====================
DataTrigger::fn_8009CB1C

Stores the value under the key when the message arrives.
====================
*/
// 0x800C926C
void DataTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == this->msg.record ) {
        fn_80028D0C( 2, "DataTrigger[%s]: Recieve %s", NULL, this->msg.record != NULL ? this->msg.record->name : NULL );
        if ( value != NULL ) {
            fn_802061E8( key, value, lbl_80527518 );
        }
        fn_80028D0C( 1, "DataTrigger[%s]: %s (%s)", NULL, key, value );
    }
}

u32 lbl_80529AE8 = fn_801A7A84( "DataTrigger" );
unk_801A8F6C lbl_80529AEC( "DataTrigger", DataTrigger::fn_800C9378, sizeof( DataTrigger ) );
