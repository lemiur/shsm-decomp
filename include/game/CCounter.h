#ifndef GAME_CCOUNTER_H
#define GAME_CCOUNTER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Counts the arrivals of its count message; when the count reaches the target, sends its
 * output message and starts again. A one-shot counter then stops counting until its reset
 * message arrives. The messages and the values are named by the attribute packet.
 * vtable __vt__8CCounter, size 0x5C. Member names invented.
 */
class CCounter : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetCountMsg,    // text: the name of the message that is counted
        CMD_SetOutputMsg,   // text: the name of the message sent when the target is reached
        CMD_SetResetMsg,    // text: the name of the message that resets the count
        CMD_SetTarget,      // integer: the count to reach
        CMD_SetOneShot      // integer: nonzero stops counting once the target is reached
    };

                    CCounter() : target( 2 ), count( 0 ), oneShot( 0 ), counting( true ) {}
    virtual         ~CCounter();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C9034( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CCounter;
    }

    unk_801A92A8    countMsg;   // 0x34
    unk_801A92A8    outputMsg;  // 0x3C
    unk_801A92A8    resetMsg;   // 0x44
    u32             target;     // 0x4C
    u32             count;      // 0x50
    u32             oneShot;    // 0x54
    bool            counting;   // 0x58

private:
    void            Send();
    void            CountReached();
    void            Count();
    void            Reset();
};

extern u32 lbl_80529AE0;            // class-name hash of "CCounter"
extern unk_801A8F6C lbl_80529AE4;   // type-registry entry of CCounter

#endif
