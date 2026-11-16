#include <game/CActorTest.h>

#include <climax/EventHandler.h>
#include <game/CBaseBehaviour.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>

#include <stdio.h>

s32         lbl_80529320;
u32         lbl_80529324 = fn_801A7A84( "CActorTest" );
unk_801A8F6C lbl_80529328( "CActorTest", CActorTest::fn_8003C384, sizeof( CActorTest ) );
s32         lbl_8052932C;

/*
====================
CActorTest::CActorTest
====================
*/
// 0x8003BE3C
CActorTest::CActorTest( const unk_AttributePacket& packet ) :
    unk_8001777C( packet )
{
    fn_801A9DE8( lbl_8052A788 );
    fn_801A9DE8( lbl_8052A790 );
    field_0x1CC = -1;
    field_0x1D0 = 0;
    field_0x1D4 = false;
    field_0x1D5 = false;
    for ( s32 i = 0; i < lbl_80529320; i++ ) {
        field_0x1D0++;
        field_0x1CC++;
    }
    sprintf( field_0x1BC, "Actor%02d", lbl_8052932C++ );
}

/*
====================
CActorTest::~CActorTest
====================
*/
// 0x8003BF4C
CActorTest::~CActorTest() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( lbl_8052A790 );
    fn_801A9E00( field_0x1D8 );
    fn_801A9940( field_0x1D8 );
    fn_801A9940( field_0x1E0 );
}

/*
====================
CActorTest::fn_801A7A88
====================
*/
// 0x8003C020
void CActorTest::fn_801A7A88() {
    unk_8001777C::fn_801A7A88();
}

/*
====================
CActorTest::fn_801A73A8
====================
*/
// 0x8003C024
void CActorTest::fn_801A73A8( const unk_AttributePacket& packet ) {
    unk_8001777C::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529324 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetField0x1D4:
            if ( it->GetU32() != 0 ) {
                field_0x1D4 = true;
            } else {
                field_0x1D4 = false;
            }
            break;
        case CMD_SetEvent1D8:
            fn_801AA218( field_0x1D8, it->GetString(), NULL );
            break;
        case CMD_SetEvent1E0:
            fn_801AA1C4( field_0x1E0, it->GetString(), NULL );
            break;
        }
        ++it;
    }
}

/*
====================
CActorTest::fn_8009CB1C
====================
*/
// 0x8003C114
void CActorTest::fn_8009CB1C( unk_Msg& msg ) {
    unk_8001777C::fn_8009CB1C( msg );
    unk_80491B38& record = GetPlayerRecord();
    if ( lbl_8052A788 == msg.record ) {
        if ( field_0x1CC != field_0x1D0 || field_0x1D5 != field_0x1D4 ) {
            s32 count = field_0x148->field_0xC;
            field_0x1CC = field_0x1D0 = ( field_0x1D0 + count ) % count;
            u32 flags = 4;
            if ( field_0x1D4 ) {
                flags |= 1;
            }
            if ( !field_0x1D5 ) {
                flags |= 8;
            }
            field_0x1D5 = field_0x1D4;
            fn_80019734( field_0x1D0, flags, 0.0f, 1.0f );
        }
        unk_8001777C::fn_80019F88( record.field_0x8 );
        if ( GetMsgName( field_0x1E0 ) != NULL && *GetMsgName( field_0x1E0 ) != '\0' ) {
            unk_Msg query( field_0x1E0, NULL );
            if ( query.record != lbl_8052A530 && query.record->head.next != NULL ) {
                fn_801AA2AC( query );
            }
            if ( query.data != NULL ) {
                fn_8009CE2C( RwFrameGetMatrix( static_cast< CBaseBehaviour* >( query.data )->field_0x30 ) );
            }
        }
        if ( field_0x1D4 ) {
            const unk_RwV3d& delta = fn_8001B200( field_0x150 );
            RwFrameGetMatrix( field_0x30 )->pos.x += delta.x;
            RwFrameGetMatrix( field_0x30 )->pos.y += delta.y;
            RwFrameGetMatrix( field_0x30 )->pos.z += delta.z;
            RwFrameUpdateObjects( field_0x30 );
        }
    } else if ( lbl_8052A790 == msg.record ) {
        unk_8001777C::fn_8001A044( record.field_0x8 );
    } else if ( field_0x1D8 == msg.record ) {
        msg.data = this;
    }
}
