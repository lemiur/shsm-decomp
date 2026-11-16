// CMessageRelay: class name from the registration strings lbl_803EC708 and lbl_803EC718.
// The inline factory fn_800CBEA0 and the two thunks follow the static initialiser in .text.

#include <game/CMessageRelay.h>
#include <game/Messages.h>
#include <stdlib.h>
#include <game/SaveData.h>
#include <game/unk_80491B38.h>

const char* lbl_80527550 = "SaveData";

/*
====================
Swap

Exchanges two values. Name invented.
====================
*/
template < class T > inline void Swap( T& a, T& b ) {
    T t = a;
    a = b;
    b = t;
}

/*
====================
Shuffle

Shuffles count messages in place, swapping each with a random one. Name invented.
====================
*/
static inline void Shuffle( unk_801A92A8* msgs, u32 count ) {
    for ( int i = 0; i != count; i++ ) {
        u32 j = rand() % count;
        Swap( msgs[i].record, msgs[j].record );
        Swap( msgs[i].links, msgs[j].links );
        Swap( msgs[i].registrations, msgs[j].registrations );
    }
}

u32 lbl_80529B40 = fn_801A7A84( "CMessageRelay" );
unk_801A8F6C lbl_80529B44( "CMessageRelay", CMessageRelay::fn_800CBEA0, sizeof( CMessageRelay ) );

/*
====================
CMessageRelay::Retrigger

Restarts the delay. Name invented.
====================
*/
inline void CMessageRelay::Retrigger() {
    fn_800A65B0( "CMessageRelay[%s]: ReTriggered while delaying - reset timer", NULL );
    delayTimer = 0.0f;
}

/*
====================
CMessageRelay::CMessageRelay
====================
*/
// 0x800CB4F8
// quirk: the attribute packet is taken and not used (the members are set later by
// fn_801A73A8), and enabled is left uninitialised until the packet sets it
CMessageRelay::CMessageRelay( const unk_AttributePacket& packet ) :
    relays( 0 ),
    limit( 0 ),
    delay( 0.0f ),
    delayTimer( 0.0f ),
    delaying( 0 ),
    cycle( false ),
    random( false ),
    retriggerResets( false ),
    cancelOnDisable( false ),
    lastOutput( -1 ),
    saveDataA( NULL ),
    saveDataB( NULL ),
    field_0xC4( NULL ) {}

/*
====================
CMessageRelay::~CMessageRelay
====================
*/
// 0x800CB5C4
CMessageRelay::~CMessageRelay() {
    if ( saveDataA != NULL ) {
        delete[] saveDataA;
    }
    if ( field_0xC4 != NULL ) {
        delete[] field_0xC4;
    }
    if ( saveDataB != NULL ) {
        delete[] saveDataB;
    }
    fn_801A9E00( triggerMsg );
    fn_801A9940( triggerMsg );
    fn_801A9E00( enableMsg );
    fn_801A9940( enableMsg );
    fn_801A9E00( disableMsg );
    fn_801A9940( disableMsg );
    for ( int i = 0; i < MAX_OUTPUTS; i++ ) {
        fn_801A9940( outputMsgs[i] );
    }
    fn_801A9E00( lbl_8052A788 );
}

/*
====================
CMessageRelay::fn_8009CB1C

Enables and disables the relay on its messages; while enabled and the save-data value is set,
relays the trigger message, at once or after the delay counted on the frame message.
====================
*/
// 0x800CB728
void CMessageRelay::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == enableMsg.record ) {
        fn_800A65B0( "CMessageRelay[%s]: Enable", NULL );
        enabled = true;
    }
    if ( msg.record == disableMsg.record ) {
        fn_800A65B0( "CMessageRelay[%s]: Disable", NULL );
        enabled = false;
        if ( delaying && cancelOnDisable ) {
            fn_800A65B0( "CMessageRelay[%s]: Trigger Cancelled On Disable ", NULL );
            delaying = 0;
            fn_801A9E00( lbl_8052A788 );
        }
    }
    if ( enabled && fn_80206348( saveDataA, saveDataB, lbl_80527550 ) && CheckKey() ) {
        if ( msg.record == triggerMsg.record ) {
            fn_80028D0C( 2, "CMessageRelay[%s]: Recieve %s", NULL,
                         triggerMsg.record != NULL ? triggerMsg.record->name : NULL );
            if ( delaying ) {
                if ( retriggerResets ) {
                    Retrigger();
                }
            } else if ( delay > 0.0f ) {
                delayTimer = 0.0f;
                delaying = 1;
                fn_801A9DE8( lbl_8052A788 );
            } else {
                fn_800CBC70();
            }
        } else if ( msg.record == lbl_8052A788.record ) {
            if ( delaying ) {
                float frameTime = lbl_80491B38.field_0x8;
                delayTimer += frameTime;
                if ( delayTimer >= delay ) {
                    delaying = 0;
                    fn_801A9E00( lbl_8052A788 );
                    fn_800CBC70();
                }
            }
        }
    }
}

/*
====================
CMessageRelay::fn_801A73A8

Takes the messages, the save-data value, the delay, the limit and the modes from the attribute
packet; a relay in both random and cycle mode shuffles its outputs once and then cycles.
====================
*/
// 0x800CB94C
void CMessageRelay::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529B40 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetSaveDataA:
            saveDataA = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetSaveDataB:
            saveDataB = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetEnabled:
            enabled = it->GetU32() != 0;
            break;
        case CMD_SetEnableMsg:
            fn_801AA218( enableMsg, it->GetString(), 0 );
            break;
        case CMD_SetDisableMsg:
            fn_801AA218( disableMsg, it->GetString(), 0 );
            break;
        case CMD_SetTriggerMsg:
            fn_801AA218( triggerMsg, it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg0:
            fn_801AA1C4( outputMsgs[0], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg1:
            fn_801AA1C4( outputMsgs[1], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg2:
            fn_801AA1C4( outputMsgs[2], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg3:
            fn_801AA1C4( outputMsgs[3], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg4:
            fn_801AA1C4( outputMsgs[4], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg5:
            fn_801AA1C4( outputMsgs[5], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg6:
            fn_801AA1C4( outputMsgs[6], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg7:
            fn_801AA1C4( outputMsgs[7], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg8:
            fn_801AA1C4( outputMsgs[8], it->GetString(), 0 );
            break;
        case CMD_SetOutputMsg9:
            fn_801AA1C4( outputMsgs[9], it->GetString(), 0 );
            break;
        case CMD_SetDelay:
            delay = it->GetFloat();
            break;
        case CMD_SetLimit:
            limit = it->GetU32();
            break;
        case CMD_SetField0xC4:
            field_0xC4 = fn_801B67FC( it->GetString(), 0 );
            break;
        // quirk: the four flag commands have no break, so each one also sets the flags of
        // the commands after it to its own value
        case CMD_SetRandom:
            if ( it->GetU32() != 0 ) {
                random = true;
            } else {
                random = false;
            }
        case CMD_SetRetriggerResets:
            if ( it->GetU32() != 0 ) {
                retriggerResets = true;
            } else {
                retriggerResets = false;
            }
        case CMD_SetCancelOnDisable:
            if ( it->GetU32() != 0 ) {
                cancelOnDisable = true;
            } else {
                cancelOnDisable = false;
            }
        case CMD_SetCycle:
            if ( it->GetU32() != 0 ) {
                cycle = true;
            } else {
                cycle = false;
            }
            break;
        }
        ++it;
    }
    if ( random && cycle ) {
        Shuffle( outputMsgs, MAX_OUTPUTS );
        random = false;
    }
}

/*
====================
CMessageRelay::fn_800CBC70

Relays the trigger: sends every registered output, or in cycle or random mode only the next
(or a random) one, skipping the last one sent and, without a delay, the trigger itself.
====================
*/
// 0x800CBC70
void CMessageRelay::fn_800CBC70() {
    relays++;
    if ( limit != 0 && relays >= limit ) {
        fn_801A9E00( triggerMsg );
        fn_801A9940( triggerMsg );
    }
    int first;
    if ( random ) {
        first = rand();
    } else {
        first = lastOutput + 1;
    }
    for ( int i = 0; i < MAX_OUTPUTS; i++ ) {
        int output = ( i + first ) % MAX_OUTPUTS;
        if ( ( random || cycle ) && output == lastOutput ) {
            output = ( lastOutput + 1 ) % MAX_OUTPUTS;
        }
        if ( delay == 0.0f && triggerMsg.record != NULL && outputMsgs[output].record == triggerMsg.record ) {
            continue;
        }
        if ( outputMsgs[output].registrations > 0 ) {
            unk_Msg msg( outputMsgs[output], NULL );
            fn_80028D0C( 4, "CMessageRelay[%s]: Sends %s", NULL, msg.record != NULL ? msg.record->name : NULL );
            if ( msg.record != lbl_8052A530.record && msg.record->head.next != NULL ) {
                fn_801AA2AC( msg );
            }
            if ( random || cycle ) {
                lastOutput = output;
                break;
            }
        }
    }
}
