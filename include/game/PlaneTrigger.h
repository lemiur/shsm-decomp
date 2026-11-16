#ifndef GAME_PLANETRIGGER_H
#define GAME_PLANETRIGGER_H

// unit name unconfirmed: the class name is from the registration strings and the log strings
// "PlaneTrigger[%s] Sends %s (Event1)" and "(Event2)"; the file name is invented.

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * A gate between two parallel planes: the planes cross the behaviour's frame at half its right
 * vector on either side, facing outwards, and the gate spans the frame's up and at vectors. When
 * the point of the trigger message (one of the AreaTriggerBase trigger messages, chosen by the
 * attribute packet) passes all the way through, entering by one plane and leaving by the other,
 * the trigger sends Event1 (left by plane A) or Event2 (left by plane B). Two more messages
 * enable and disable it. The class name has no C prefix in the registration strings. vtable
 * __vt__12PlaneTrigger, size 0xB8. Member names invented.
 */
class PlaneTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetEnabled,         // integer: nonzero enables the trigger
        CMD_SetEnableMsg,       // text: the message that enables it
        CMD_SetDisableMsg,      // text: the message that disables it
        CMD_SetEvent1,          // text: the message sent on leaving by plane A
        CMD_SetEvent2,          // text: the message sent on leaving by plane B
        CMD_SetTriggerType = 6  // integer: which trigger message moves the point (TRIGGER_)
    };

    /* The trigger messages, in attribute order. Names invented. */
    enum {
        TRIGGER_Player,         // lbl_80529C08, "player_trigger"
        TRIGGER_Enemy,          // lbl_80529C10, "enemy_trigger"
        TRIGGER_Boss,           // lbl_80529C18, "boss_trigger"
        TRIGGER_RaceStartLine   // lbl_80529C20, "race_start_line_trigger"
    };

                    PlaneTrigger() :
                        triggerMsg( NULL ),
                        distA( 0.0f ),
                        distB( 0.0f ),
                        tracking( false ),
                        pendingEvent1( false ),
                        pendingEvent2( false ) {}
    virtual         ~PlaneTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_800CE238( unk_Msg& msg );    // called with each event before it is sent

    static CAttributeHandler* fn_800CE5C8( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) PlaneTrigger;
    }

    unk_801A92A8*   triggerMsg;     // 0x34, the trigger message listened to; NULL for none
    unk_801A92A8    event1;         // 0x38
    unk_801A92A8    event2;         // 0x40
    float           distA;          // 0x48, the point's last signed distance outside plane A
    float           distB;          // 0x4C, the same for plane B
    bool            tracking;       // 0x50, distA and distB hold a point
    bool            pendingEvent1;  // 0x51, the point came in by plane B: leaving by A sends Event1
    bool            pendingEvent2;  // 0x52, the point came in by plane A: leaving by B sends Event2
    unk_RwV3d       pointA;         // 0x54, a point of plane A
    unk_RwV3d       pointB;         // 0x60, a point of plane B
    unk_RwV3d       normalA;        // 0x6C, plane A's outward unit normal (against the frame's right vector)
    unk_RwV3d       normalB;        // 0x78, plane B's (along the right vector)
    unk_RwV3d       upAxis;         // 0x84, the frame's up vector, normalised
    unk_RwV3d       atAxis;         // 0x90, the frame's at vector, normalised
    float           halfHeight;     // 0x9C, half the gate's extent along upAxis
    float           halfDepth;      // 0xA0, half the gate's extent along atAxis
    unk_801A92A8    enableMsg;      // 0xA4
    unk_801A92A8    disableMsg;     // 0xAC
    bool            enabled;        // 0xB4, not initialised by the constructor
};

extern unk_801A8F6C lbl_80529BA0;   // type-registry entry of PlaneTrigger
extern u32 lbl_80529BA4;            // class-name hash of "PlaneTrigger"

#endif
