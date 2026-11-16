#ifndef GAME_CPOINTTRIGGER_H
#define GAME_CPOINTTRIGGER_H

// unit name unconfirmed: the class name is evidenced by the registration strings lbl_803EBBD8 and
// lbl_803EBBE8 and the log string "CPointTrigger[%s] sends %s & %s"; the file name is invented.

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/CPointOfInterest.h>

/*
 * A point of interest that sends two messages when something triggers it (fn_800C62B0), as
 * long as its save-data value (fn_80206348) is set. With field_0x94 set the trigger is handed
 * to fn_80068614 instead. vtable lbl_803EBB28, size 0x98. Its inline overrides are emitted out
 * of line by CLookTrigger's unit, which links first. Member names invented.
 */
class CPointTrigger : public CPointOfInterest, public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetMsgA,            // text: the first message sent
        CMD_SetMsgB,            // text: the second message sent
        CMD_SetSaveDataA,       // text: copied to saveDataA
        CMD_SetSaveDataB,       // text: copied to saveDataB
        CMD_SetField0x88,       // text: copied to field_0x88
        CMD_SetField0x94 = 9    // integer: nonzero hands triggers to fn_80068614
    };

                    CPointTrigger( const unk_AttributePacket& packet );
    virtual         ~CPointTrigger();
    virtual SClassInfo* fn_800C4E9C() { return CAttributeHandler::mClassInfo; }
    virtual CZone*  fn_800C4E8C() { return CAttributeHandler::mZone; }
    virtual CZone*  fn_800C4E94() { return CAttributeHandler::mZone; }
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_800C6274( unk_8003C454* who );
    virtual int     fn_800C54EC() { return fn_800C63BC(); }
    virtual void    fn_800C62B0( unk_8003C454* who );

    void            fn_800C62A0( unk_8003C454* who );
    bool            fn_800C63AC();
    int             fn_800C63BC();

    static CAttributeHandler* fn_800C6430( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPointTrigger( packet );
    }

    unk_801A92A8    msgA;           // 0x78
    unk_801A92A8    msgB;           // 0x80
    char*           field_0x88;     // 0x88, owned copy; a third save-data key whose test is always true here
    char*           saveDataA;      // 0x8C, owned copy; first argument of fn_80206348
    char*           saveDataB;      // 0x90, owned copy; second argument of fn_80206348
    bool            field_0x94;     // 0x94
};

// defined in 0x80068614, unit not yet reconstructed: takes over a trigger of a point with
// field_0x94 set. Identity TODO: free function or a member of who's class.
void fn_80068614( unk_8003C454* who, CPointTrigger* trigger );

extern const char* lbl_805274A0;    // "SaveData", the group of the save-data value
extern u32 lbl_80529A90;            // class-name hash of "CPointTrigger"
extern unk_801A8F6C lbl_80529A94;   // type-registry entry of CPointTrigger

#endif
