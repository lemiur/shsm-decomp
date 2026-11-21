// CSplineHelper: class name from the registration string "CSplineHelper" in the static initialiser.
// The factory fn_800A8024 is the header's static member; the thunks follow the destructor in .text.

#include <game/CSplineHelper.h>
#include <game/CSystemCommands.h>

namespace UTILS {
bool StringsEqual( const char* a, const char* b );
}

extern "C" void fn_8013CEF8( unk_SplineOps** ops );
extern "C" const unk_SplineRecord* fn_801ABE14( const char* name, const char** outName, u32 flags,
                                                const unk_AttributeCommand** outCommand );

u32 lbl_80529838 = fn_801A7A84( "CSplineHelper" );
unk_801A8F6C lbl_8052983C( "CSplineHelper", CSplineHelper::fn_800A8024, sizeof( CSplineHelper ) );

/*
====================
CSplineHelper::~CSplineHelper

Posts the destruction message on postMsg, unlinks the handler from listenMsg and unregisters both ids.
====================
*/
// 0x800A7CF8
CSplineHelper::~CSplineHelper() {
    unk_SplinePayload payload;
    payload.owner = this;
    payload.alive = false;
    PostMsg( postMsg, &payload );
    fn_801A9E00( listenMsg );
    fn_801A9940( listenMsg );
    fn_801A9940( postMsg );
}

/*
====================
CSplineHelper::fn_8009CB1C

Passes the message on to the base, then posts postMsg with the helper when the message is listenMsg.
====================
*/
// 0x800A7DE0
void CSplineHelper::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == listenMsg.record ) {
        unk_SplinePayload payload;
        payload.owner = this;
        payload.alive = true;
        PostMsg( postMsg, &payload );
    }
}

/*
====================
CSplineHelper::fn_801A73A8

Reads the packet's commands of this class: command 0 links the handler to listenMsg and command 1
registers postMsg, both under the command's name. Then, for the rwID_SPLINE record of the other
class, it takes the record's value and initialises the table at 0x48.
====================
*/
// 0x800A7E64
void CSplineHelper::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    for ( unk_AttributeCommandIterator it( packet, lbl_80529838 ); !it.IsFinished(); ++it ) {
        switch ( it->id ) {
        case 0:
            fn_801AA218( listenMsg, it->GetString(), NULL );
            break;
        case 1:
            fn_801AA1C4( postMsg, it->GetString(), NULL );
            break;
        }
    }
    for ( unk_AttributeCommandIterator it( packet, lbl_805297E8 ); !it.IsFinished(); ++it ) {
        if ( it->id == 0 ) {
            const char* name;
            const unk_AttributeCommand* command;
            const unk_SplineRecord* record = fn_801ABE14( it->GetString(), &name, 0, &command );
            if ( record != NULL && UTILS::StringsEqual( "rwID_SPLINE", name ) ) {
                u32 value = record->value;
                field_0x44 = value;
                field_0x64 = value;
                fn_8013CEF8( &field_0x48 );
            }
        }
    }
}
