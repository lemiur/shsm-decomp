#ifndef GAME_CSLOWMOTRIGGER_H
#define GAME_CSLOWMOTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Slows the game down when its start message arrives: sets the time scale, and when a duration
 * is given, restores normal speed after it has run out; the stop message restores it at once.
 * A one-shot trigger unregisters both messages when it stops. The messages and the values are
 * named by the attribute packet. vtable __vt__14CSlowMoTrigger, size 0x5C. Member names invented.
 */
class CSlowMoTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetStartMsg,        // text: the name of the message that starts the slow motion
        CMD_SetStopMsg,         // text: the name of the message that stops it
        CMD_SetBlendInTime,     // float: the time taken to blend to the time scale
        CMD_SetTimeScale,       // float: the time scale
        CMD_SetBlendOutTime,    // float: the time taken to blend back to normal speed
        CMD_SetDuration,        // float: seconds until it stops by itself, 0 for no limit
        CMD_SetOneShot          // integer: nonzero unregisters the messages when it stops
    };

                    CSlowMoTrigger()
                        : blendInTime( 0.0f ), blendOutTime( 0.0f ), timeScale( 1.0f ),
                          duration( 0.0f ), oneShot( 0 ) {}
    virtual         ~CSlowMoTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CF8B8( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSlowMoTrigger;
    }

    unk_801A92A8    startMsg;       // 0x34
    unk_801A92A8    stopMsg;        // 0x3C
    float           blendInTime;    // 0x44
    float           blendOutTime;   // 0x48
    float           timeScale;      // 0x4C
    float           duration;       // 0x50
    u32             oneShot;        // 0x54
    float           timeLeft;       // 0x58, counts down while the slow motion runs

private:
    void            Start();
    void            Stop();
};

extern u32 lbl_80529BD0;            // class-name hash of "CSlowMoTrigger"
extern unk_801A8F6C lbl_80529BD4;   // type-registry entry of CSlowMoTrigger

#endif
