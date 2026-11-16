#ifndef GAME_CFLICKERTRIGGER_H
#define GAME_CFLICKERTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * The flicker settings CFlickerTrigger sends as its message payload; the member names follow its
 * debug format ("Rate=..., Intensity=...->..., Bias=..., Time=..., LowT/R=.../..."). Name invented.
 */
struct unk_FlickerParams {
    float   rate;           // 0x0
    float   intensityFrom;  // 0x4
    float   intensityTo;    // 0x8
    float   bias;           // 0xC
    float   time;           // 0x10
    float   lowTime;        // 0x14
    float   lowRate;        // 0x18
};

/*
 * Sends its flicker settings with its send message when its trigger message arrives; the messages
 * and the settings are named by the attribute packet. vtable __vt__15CFlickerTrigger, size 0x60.
 * Member names invented.
 */
class CFlickerTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,      // text: the name of the message that triggers the send
        CMD_SetSendMsg,         // text: the name of the message sent
        CMD_SetRate,            // float
        CMD_SetIntensityFrom,   // float
        CMD_SetIntensityTo,     // float
        CMD_SetBias,            // float
        CMD_SetLowTime,         // float
        CMD_SetLowRate,         // float
        CMD_SetTime             // float
    };

                    CFlickerTrigger() {
                        params.rate = 10.0f;
                        params.intensityFrom = 0.0f;
                        params.intensityTo = 1.0f;
                    }
    virtual         ~CFlickerTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C9F9C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CFlickerTrigger;
    }

    unk_801A92A8        triggerMsg; // 0x34
    unk_801A92A8        sendMsg;    // 0x3C
    unk_FlickerParams   params;     // 0x44
};

extern u32 lbl_80529B08;            // class-name hash of "CFlickerTrigger"
extern unk_801A8F6C lbl_80529B0C;   // type-registry entry of CFlickerTrigger

#endif
