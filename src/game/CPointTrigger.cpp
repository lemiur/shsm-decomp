// unit name unconfirmed: the class name is from the registration strings lbl_803EBBD8 and
// lbl_803EBBE8; the file name is invented. The inline factory fn_800C6430 and the three thunks
// follow the static initialiser in .text.

#include <game/CPointTrigger.h>
#include <game/Log.h>
#include <game/SaveData.h>

const char* lbl_805274A0 = "SaveData";

u32 lbl_80529A90 = fn_801A7A84( "CPointTrigger" );
unk_801A8F6C lbl_80529A94( "CPointTrigger", CPointTrigger::fn_800C6430, sizeof( CPointTrigger ) );

/*
====================
CPointTrigger::CPointTrigger
====================
*/
// 0x800C5FCC
CPointTrigger::CPointTrigger( const unk_AttributePacket& packet ) :
    CPointOfInterest( packet ),
    field_0x88( NULL ),
    saveDataA( NULL ),
    saveDataB( NULL ),
    field_0x94( false ) {
    matrix = RwFrameGetMatrix( CBaseBehaviour::field_0x30 );
}

/*
====================
CPointTrigger::~CPointTrigger
====================
*/
// 0x800C6058
CPointTrigger::~CPointTrigger() {
    delete field_0x88;
    delete saveDataA;
    delete saveDataB;
    fn_801A9940( msgA );
    fn_801A9940( msgB );
}

/*
====================
CPointTrigger::fn_801A73A8

Takes the two messages, the save-data value, the third key and field_0x94 from the attribute
packet.
====================
*/
// 0x800C6124
void CPointTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    CPointOfInterest::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529A90 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetMsgA:
            fn_801AA1C4( msgA, it->GetString(), 0 );
            break;
        case CMD_SetMsgB:
            fn_801AA1C4( msgB, it->GetString(), 0 );
            break;
        case CMD_SetSaveDataA:
            saveDataA = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetSaveDataB:
            saveDataB = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField0x88:
            field_0x88 = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetField0x94:
            if ( it->GetU32() != 0 ) {
                field_0x94 = true;
            } else {
                field_0x94 = false;
            }
            break;
        }
        ++it;
    }
}

/*
====================
CPointTrigger::fn_800C6274

Triggers the point, or with field_0x94 set hands the trigger to fn_80068614.
====================
*/
// 0x800C6274
void CPointTrigger::fn_800C6274( unk_8003C454* who ) {
    if ( field_0x94 ) {
        fn_80068614( who, this );
    } else {
        fn_800C62B0( who );
    }
}

/*
====================
CPointTrigger::fn_800C62A0
====================
*/
// 0x800C62A0
void CPointTrigger::fn_800C62A0( unk_8003C454* who ) {
    fn_800C62B0( who );
}

/*
====================
CPointTrigger::fn_800C62B0

Sends both messages while the save-data value is set.
====================
*/
// 0x800C62B0
void CPointTrigger::fn_800C62B0( unk_8003C454* who ) {
    if ( fn_800C63BC() && fn_800C63AC() ) {
        const char* nameA = msgA.record != NULL ? msgA.record->name : NULL;
        const char* nameB = msgB.record != NULL ? msgB.record->name : NULL;
        fn_80028D0C( 4, "CPointTrigger[%s] sends %s & %s", NULL, nameA, nameB );
        PostMsg( msgA, NULL );
        PostMsg( msgB, NULL );
    }
}

/*
====================
CPointTrigger::fn_800C63AC

The test of the third key, whose debug check (CheckSaveDataKey) is compiled out in release.
====================
*/
// 0x800C63AC
bool CPointTrigger::fn_800C63AC() {
    if ( field_0x88 != NULL ) {
        return CheckSaveDataKey( field_0x88 );
    }
    return true;
}

/*
====================
CPointTrigger::fn_800C63BC

Whether the save-data value is set.
====================
*/
// 0x800C63BC
int CPointTrigger::fn_800C63BC() {
    return fn_80206348( saveDataA, saveDataB, lbl_805274A0 );
}
