#ifndef GAME_CPADRUMBLETRIGGER_H
#define GAME_CPADRUMBLETRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Starts a big or small pad rumble when its message arrives and stops it again once its
 * lifetime has run out; the message, the kind of rumble and the rumble values are set by the
 * attribute packet. Its start and stop payloads are the "RumbleStartData*" of the pad
 * manager's messages: only the first two members of the stop payload are used (they end at
 * 0x58, where the timer lies). vtable __vt__17CPadRumbleTrigger, size 0x60. Member names invented.
 */
class CPadRumbleTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetRumbleMsg,       // text: the name of the message that starts the rumble
        CMD_SetSmall,           // integer: nonzero for the small rumble, 0 for the big one
        CMD_SetStartLevel,      // float: startLevel
        CMD_SetStartLifetime,   // float: startLifetime
        CMD_SetStartRamp,       // float: startRamp
        CMD_SetStopLevel        // float: stopLevel
    };

                    CPadRumbleTrigger() :
                        startPad( 0 ),
                        startLevel( 0.0f ),
                        startLifetime( -1.0f ),
                        startRamp( 0.0f ),
                        stopLevel( 0.0f ),
                        active( false ) {
                        // the pads are set again after the member initialisers, as retail does
                        startPad = 0;
                        stopPad = 0;
                    }
    virtual         ~CPadRumbleTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CD2CC( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPadRumbleTrigger;
    }

    unk_801A92A8    rumbleMsg;      // 0x34
    u32             isSmall;        // 0x3C, nonzero: the small rumble
    int             startPad;       // 0x40, the start payload begins here
    float           startLevel;     // 0x44
    float           startLifetime;  // 0x48, seconds; negative never stops
    float           startRamp;      // 0x4C
    int             stopPad;        // 0x50, the stop payload begins here
    float           stopLevel;      // 0x54
    float           timeLeft;       // 0x58, seconds until the rumble stops
    bool            active;         // 0x5C, listening to lbl_8052A788 for the frame time
};

extern u32 lbl_80529B78;            // class-name hash of "CPadRumbleTrigger"
extern unk_801A8F6C lbl_80529B7C;   // type-registry entry of CPadRumbleTrigger

#endif
