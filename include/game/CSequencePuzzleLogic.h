#ifndef GAME_CSEQUENCEPUZZLELOGIC_H
#define GAME_CSEQUENCEPUZZLELOGIC_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Messages.h>
#include <game/SaveData.h>
#include <game/unk_Deque.h>

/*
 * A puzzle solved by receiving its input messages in a sequence. Each input received is
 * recorded, the latest first, keeping the last historyLength; the puzzle is solved when the
 * last sequenceLength inputs are inputs 0 to sequenceLength - 1, in that order in ordered
 * mode, in any order otherwise. Solving sends the output message, at once or after the delay
 * counted on the frame message lbl_8052A788; a limit unlinks the inputs after that many
 * solutions. Two more messages enable and disable the puzzle, and a save-data value
 * (fn_80206348) must be set for it to listen. The history is saved as text, one letter per
 * input from 'a'. vtable __vt__20CSequencePuzzleLogic, size 0xE0. Member names invented.
 */
class CSequencePuzzleLogic : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetOutputMsg,           // text: the name of the message sent when solved
        CMD_SetInputMsg0,           // text: the names of the input messages, one command each
        CMD_SetInputMsg1,
        CMD_SetInputMsg2,
        CMD_SetInputMsg3,
        CMD_SetInputMsg4,
        CMD_SetInputMsg5,
        CMD_SetInputMsg6,
        CMD_SetInputMsg7,
        CMD_SetInputMsg8,
        CMD_SetInputMsg9,
        CMD_SetHistoryLength,       // integer: the inputs kept
        CMD_SetSequenceLength,      // integer: the inputs that make the sequence
        CMD_SetOrdered,             // integer: nonzero needs the inputs in order
        CMD_SetSaveDataA,           // text: copied to saveDataA
        CMD_SetSaveDataB,           // text: copied to saveDataB
        CMD_SetEnabled,             // integer: nonzero enables the puzzle
        CMD_SetEnableMsg,           // text: the name of the message that enables the puzzle
        CMD_SetDisableMsg,          // text: the name of the message that disables the puzzle
        CMD_SetDelay,               // float: the delay before the output is sent
        CMD_SetLimit,               // integer: the solutions before the inputs are unlinked; 0 for none
        CMD_SetField0xDC            // text: copied to field_0xDC
    };

    enum { MAX_INPUTS = 10 };
    enum { HISTORY_BLOCK_SIZE = 64 };
    enum { SAVE_TEXT_SIZE = 16 };   // the saved history text, its terminator included

                    CSequencePuzzleLogic( const unk_AttributePacket& packet );
    virtual         ~CSequencePuzzleLogic();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_8000CFD0();
    virtual void    fn_8000CFCC();
    bool            fn_80170C54();

    static CAttributeHandler* fn_80170E8C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSequencePuzzleLogic( packet );
    }

    unk_801A92A8    outputMsg;                  // 0x34
    unk_801A92A8    inputMsgs[MAX_INPUTS];      // 0x3C
    u32             historyLength;              // 0x8C, the inputs kept
    u32             sequenceLength;             // 0x90
    unk_Deque< u8, HISTORY_BLOCK_SIZE > history;    // 0x94, the inputs received, the latest first
    u32             solutions;                  // 0xAC, solutions so far
    u32             limit;                      // 0xB0
    float           delay;                      // 0xB4
    float           delayTimer;                 // 0xB8
    int             delaying;                   // 0xBC, the output waits for the delay
    bool            enabled;                    // 0xC0
    bool            ordered;                    // 0xC1
    unk_801A92A8    enableMsg;                  // 0xC4
    unk_801A92A8    disableMsg;                 // 0xCC
    char*           saveDataA;                  // 0xD4, owned copy; first argument of fn_80206348
    char*           saveDataB;                  // 0xD8, owned copy; second argument of fn_80206348
    char*           field_0xDC;                 // 0xDC, owned copy; a third save-data key whose test is always true here

private:
    // The helpers below follow the PS2 build (SLUS_218.99), where they are separate functions
    // (OnInput 0x25FBB8, Solve 0x25FF40, Save 0x260028, SetHistory 0x260170, Load 0x2606D0,
    // Update 0x260620, UnlinkInputs 0x260588). On the Wii every call is inlined; the four
    // defined in the .cpp leave dead-stripped copies whose string literals stay in .data.
    // Names invented.
    void            OnInput( int input );
    void            Solve();
    void            Load();
    void            Save();

    /* Replaces the history with the saved text, one letter per input from 'a'. */
    void            SetHistory( const char* text ) {
        history.clear();
        while ( *text != '\0' ) {
            history.push_back( *text++ - 'a' );
        }
    }

    /* Releases the input messages. */
    void            UnlinkInputs() {
        for ( int i = 0; i < MAX_INPUTS; i++ ) {
            fn_801A9E00( inputMsgs[i] );
            fn_801A9940( inputMsgs[i] );
        }
    }

    /* Counts the delay on the frame message; sends the output when it has passed. */
    void            Update( float frameTime ) {
        delayTimer += frameTime;
        if ( delayTimer >= delay ) {
            delaying = 0;
            fn_801A9E00( lbl_8052A788 );
            Solve();
        }
    }

    // The test of the third key, whose debug check is compiled out in release. Name invented.
    bool            CheckKey() {
        if ( field_0xDC != NULL ) {
            return CheckSaveDataKey( field_0xDC );
        }
        return true;
    }
};

extern const char* lbl_805284B8;    // "SaveData", the group of the save-data value
extern u32 lbl_8052A2C0;            // class-name hash of "CSequencePuzzleLogic"
extern unk_801A8F6C lbl_8052A2C4;   // type-registry entry of CSequencePuzzleLogic

#endif
