#ifndef GAME_CMESSAGERELAY_H
#define GAME_CMESSAGERELAY_H

// unit name unconfirmed: the class name is evidenced by the registration strings lbl_803EC708 and
// lbl_803EC718 (and Ghost Rider's class-name literals); the file name is invented.

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Log.h>
#include <game/SaveData.h>

/*
 * Relays its trigger message to up to ten output messages. Each trigger sends one output
 * message per registered output: all of them, or, in cycle mode, the next one after the last
 * sent (in random mode a random one). A delay holds the relay back, counted on the frame
 * message lbl_8052A788; a limit unlinks the trigger after that many relays. Two more messages
 * enable and disable the relay, and a save-data value (fn_80206348) must be set for it to
 * relay. vtable __vt__13CMessageRelay, size 0x58. Member names invented.
 */
class CMessageRelay : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,          // text: the name of the message that is relayed
        CMD_SetOutputMsg0,          // text: the names of the output messages, one command each
        CMD_SetOutputMsg1,
        CMD_SetOutputMsg2,
        CMD_SetOutputMsg3,
        CMD_SetOutputMsg4,
        CMD_SetOutputMsg5,
        CMD_SetOutputMsg6,
        CMD_SetOutputMsg7,
        CMD_SetOutputMsg8,
        CMD_SetOutputMsg9,
        CMD_SetSaveDataA,           // text: copied to saveDataA
        CMD_SetSaveDataB,           // text: copied to saveDataB
        CMD_SetEnabled,             // integer: nonzero enables the relay
        CMD_SetEnableMsg,           // text: the name of the message that enables the relay
        CMD_SetDisableMsg,          // text: the name of the message that disables the relay
        CMD_SetDelay,               // float: the delay before relaying
        CMD_SetLimit,               // integer: the relays before the trigger is unlinked; 0 for none
        CMD_SetField0xC4,           // text: copied to field_0xC4
        CMD_SetRandom,              // integer: nonzero picks the output at random
        CMD_SetRetriggerResets,     // integer: nonzero restarts the delay on a trigger
        CMD_SetCancelOnDisable,     // integer: nonzero drops a delayed relay when disabled
        CMD_SetCycle                // integer: nonzero sends one output per trigger, in turn
    };

    enum { MAX_OUTPUTS = 10 };

                    CMessageRelay( const unk_AttributePacket& packet );
    virtual         ~CMessageRelay();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    void            fn_800CBC70();

    static CAttributeHandler* fn_800CBEA0( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CMessageRelay( packet );
    }

    unk_801A92A8    triggerMsg;                 // 0x34
    unk_801A92A8    outputMsgs[MAX_OUTPUTS];    // 0x3C
    u32             relays;                     // 0x8C, relays so far
    u32             limit;                      // 0x90
    float           delay;                      // 0x94
    float           delayTimer;                 // 0x98
    int             delaying;                   // 0x9C, a relay waits for the delay
    bool            cycle;                      // 0xA0
    bool            random;                     // 0xA1
    bool            enabled;                    // 0xA2
    bool            retriggerResets;            // 0xA3
    bool            cancelOnDisable;            // 0xA4
    int             lastOutput;                 // 0xA8, the output sent last; -1 before the first
    unk_801A92A8    enableMsg;                  // 0xAC
    unk_801A92A8    disableMsg;                 // 0xB4
    char*           saveDataA;                  // 0xBC, owned copy; first argument of fn_80206348
    char*           saveDataB;                  // 0xC0, owned copy; second argument of fn_80206348
    char*           field_0xC4;                 // 0xC4, owned copy; a third save-data key whose test is always true here

private:
    void            Retrigger();

    // The test of the third key, whose debug check is compiled out in release. Name invented.
    bool            CheckKey() {
        if ( field_0xC4 != NULL ) {
            return CheckSaveDataKey( field_0xC4 );
        }
        return true;
    }
};

extern const char* lbl_80527550;    // "SaveData", the group of the save-data value
extern u32 lbl_80529B40;            // class-name hash of "CMessageRelay"
extern unk_801A8F6C lbl_80529B44;   // type-registry entry of CMessageRelay

#endif
