// CSequencePuzzleLogic: class name from the registration strings lbl_803FADD0 and lbl_803FADE8
// and the log strings "CSequencePuzzleLogic[%s]: ...". The inline factory fn_80170E8C and the two
// thunks follow the static initialiser in .text.

#include <game/CSequencePuzzleLogic.h>
#include <game/Log.h>
#include <game/Messages.h>
#include <game/SaveData.h>
#include <game/unk_800264AC.h>
#include <game/unk_80491B38.h>

const char* lbl_805284B8 = "SaveData";

u32 lbl_8052A2C0 = fn_801A7A84( "CSequencePuzzleLogic" );
unk_801A8F6C lbl_8052A2C4( "CSequencePuzzleLogic", CSequencePuzzleLogic::fn_80170E8C, sizeof( CSequencePuzzleLogic ) );

/*
====================
CSequencePuzzleLogic::CSequencePuzzleLogic
====================
*/
// 0x8016FB50
// quirk: the attribute packet is taken and not used (the members are set later by
// fn_801A73A8)
CSequencePuzzleLogic::CSequencePuzzleLogic( const unk_AttributePacket& packet ) :
    historyLength( 0 ),
    sequenceLength( 0 ),
    solutions( 0 ),
    limit( 0 ),
    delay( 0.0f ),
    delayTimer( 0.0f ),
    delaying( 0 ),
    enabled( true ),
    ordered( false ),
    saveDataA( NULL ),
    saveDataB( NULL ),
    field_0xDC( NULL ) {}

/*
====================
CSequencePuzzleLogic::~CSequencePuzzleLogic
====================
*/
// 0x8016FC84
CSequencePuzzleLogic::~CSequencePuzzleLogic() {
    if ( saveDataA != NULL ) {
        delete[] saveDataA;
    }
    if ( field_0xDC != NULL ) {
        delete[] field_0xDC;
    }
    if ( saveDataB != NULL ) {
        delete[] saveDataB;
    }
    fn_801A9940( outputMsg );
    fn_801A9E00( enableMsg );
    fn_801A9940( enableMsg );
    fn_801A9E00( disableMsg );
    fn_801A9940( disableMsg );
    UnlinkInputs();
    fn_801A9E00( lbl_8052A788 );
}

/*
====================
CSequencePuzzleLogic::fn_8009CB1C

Enables and disables the puzzle on its messages; while enabled and the save-data value is set,
records each input received and, when the sequence is complete, solves the puzzle, at once or
after the delay counted on the frame message.
====================
*/
// NonMatching: remaining difference not yet diagnosed
// Not a const-parameter effect: msg's type is fixed by the CBaseBehaviour virtual, and the rows that move are
// accesses to this and register choices.
// 0x8016FEC0
void CSequencePuzzleLogic::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == enableMsg.record ) {
        fn_800A65B0( "CSequencePuzzleLogic[%s]: Enable", NULL );
        enabled = true;
    }
    if ( msg.record == disableMsg.record ) {
        fn_800A65B0( "CSequencePuzzleLogic[%s]: Disable", NULL );
        enabled = false;
    }
    if ( enabled && fn_80206348( saveDataA, saveDataB, lbl_805284B8 ) && CheckKey() ) {
        if ( msg.record == lbl_8052A788.record ) {
            if ( delaying ) {
                Update( lbl_80491B38.field_0x8 );
            }
        } else {
            for ( int i = 0; i < MAX_INPUTS; i++ ) {
                if ( msg.record == inputMsgs[i].record ) {
                    fn_80028D0C( 2, "CSequencePuzzleLogic[%s]: Recieve %s", NULL,
                                 inputMsgs[i].record != NULL ? inputMsgs[i].record->name : NULL );
                    OnInput( i );
                }
            }
        }
    }
}

/*
====================
CSequencePuzzleLogic::fn_801A73A8

Takes the messages, the save-data value, the lengths, the delay, the limit and the modes from
the attribute packet.
====================
*/
// 0x801704E4
void CSequencePuzzleLogic::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_8052A2C0 );
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
        case CMD_SetOutputMsg:
            fn_801AA1C4( outputMsg, it->GetString(), 0 );
            break;
        case CMD_SetInputMsg0:
            fn_801AA218( inputMsgs[0], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg1:
            fn_801AA218( inputMsgs[1], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg2:
            fn_801AA218( inputMsgs[2], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg3:
            fn_801AA218( inputMsgs[3], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg4:
            fn_801AA218( inputMsgs[4], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg5:
            fn_801AA218( inputMsgs[5], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg6:
            fn_801AA218( inputMsgs[6], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg7:
            fn_801AA218( inputMsgs[7], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg8:
            fn_801AA218( inputMsgs[8], it->GetString(), 0 );
            break;
        case CMD_SetInputMsg9:
            fn_801AA218( inputMsgs[9], it->GetString(), 0 );
            break;
        case CMD_SetDelay:
            delay = it->GetFloat();
            break;
        case CMD_SetLimit:
            limit = it->GetU32();
            break;
        case CMD_SetField0xDC:
            field_0xDC = fn_801B67FC( it->GetString(), 0 );
            break;
        case CMD_SetHistoryLength:
            historyLength = it->GetU32();
            break;
        case CMD_SetSequenceLength:
            sequenceLength = it->GetU32();
            break;
        case CMD_SetOrdered:
            if ( it->GetU32() != 0 ) {
                ordered = true;
            } else {
                ordered = false;
            }
            break;
        }
        ++it;
    }
}

/*
====================
CSequencePuzzleLogic::fn_8000CFD0

Loads the history from the instance's save block.
====================
*/
// NonMatching: this and text swap registers, and DestroyAll's iterator and end() temporaries sit in
// swapped stack slots
// 0x8017076C
void CSequencePuzzleLogic::fn_8000CFD0() {
    Load();
}

/*
====================
CSequencePuzzleLogic::fn_8000CFCC

Saves the history to the instance's save block.
====================
*/
// NonMatching: size()'s iterators are built begin-first, retail end-first, but with the stack slots
// of end() - begin() (begin's copies at 0x08/0x18). A non-member operator-, a named end() local, a
// distance helper taking (begin(), end()) by value or by reference, and a by-value operator- all
// load end first but move the slots or registers.
// 0x80170B14
void CSequencePuzzleLogic::fn_8000CFCC() {
    Save();
}

/*
====================
CSequencePuzzleLogic::OnInput

Records an input, the latest first, keeping the last historyLength; when the sequence is
complete and no delay runs, solves the puzzle at once or starts the delay. Inlined into the
message handler (dead-stripped copy).
====================
*/
void CSequencePuzzleLogic::OnInput( int input ) {
    history.push_front( input );
    if ( history.size() > historyLength ) {
        history.pop_back();
    }
    if ( fn_80170C54() && !delaying ) {
        if ( delay > 0.0f ) {
            delayTimer = 0.0f;
            delaying = 1;
            fn_801A9DE8( lbl_8052A788 );
        } else {
            Solve();
        }
    }
}

/*
====================
CSequencePuzzleLogic::Solve

Sends the output message; a limit reached unlinks the inputs first. Inlined into the message
handler (dead-stripped copy).
====================
*/
void CSequencePuzzleLogic::Solve() {
    solutions++;
    if ( limit != 0 && solutions >= limit ) {
        UnlinkInputs();
    }
    fn_80028D0C( 4, "CSequencePuzzleLogic[%s]: Sends %s", NULL, outputMsg.record != NULL ? outputMsg.record->name : NULL );
    if ( outputMsg.record != lbl_8052A530.record && outputMsg.record->head.next != NULL ) {
        unk_Msg msg( outputMsg, NULL );
        fn_801AA2AC( msg );
    }
}

/*
====================
CSequencePuzzleLogic::Save

Saves the history to the instance's save block as text, one letter per input from 'a'.
Inlined into fn_8000CFCC (dead-stripped copy).
====================
*/
void CSequencePuzzleLogic::Save() {
    char text[SAVE_TEXT_SIZE];
    char* p = text;
    for ( u32 i = 0; i != history.size(); i++ ) {
        *p++ = history[i] + 'a';
    }
    *p = '\0';
    void* root = fn_802050AC( fn_80204C04(), "GWI", 1, 0, 0, 0 );
    void* save = fn_802050AC( root, fn_800264AC( mZone ), 1, 0, 0, 0 );
    fn_80205354( save, "CSequencePuzzleLogic", 5, text, 0 );
}

/*
====================
CSequencePuzzleLogic::Load

Loads the history from the instance's save block, when it holds one. Inlined into
fn_8000CFD0 (dead-stripped copy).
====================
*/
void CSequencePuzzleLogic::Load() {
    void* root = fn_802050AC( fn_80204C04(), "GWI", 1, 0, 0, 0 );
    void* save = fn_802050AC( root, fn_800264AC( mZone ), 1, 0, 0, 0 );
    const char* text = NULL;
    fn_802057FC( save, "CSequencePuzzleLogic", 5, &text, 0 );
    if ( text != NULL ) {
        SetHistory( text );
    }
}

/*
====================
CSequencePuzzleLogic::fn_80170C54

Whether the history holds the sequence: its latest sequenceLength inputs are inputs 0 to
sequenceLength - 1 in order (the latest first), or in ordered mode off, each of them anywhere.
====================
*/
// NonMatching: retail hoists more iterator copies in the unordered search and tests the ordered
// count with beq
// 0x80170C54
bool CSequencePuzzleLogic::fn_80170C54() {
    bool solved = false;
    if ( history.size() >= sequenceLength ) {
        solved = true;
        if ( ordered ) {
            for ( u32 i = 0; i < sequenceLength; i++ ) {
                if ( history[i] != sequenceLength - ( i + 1 ) ) {
                    solved = false;
                    break;
                }
            }
        } else {
            for ( int i = 0; i < sequenceLength; i++ ) {
                if ( unk_Find( history.begin(), history.end(), i ) == history.end() ) {
                    solved = false;
                    break;
                }
            }
        }
    }
    return solved;
}
