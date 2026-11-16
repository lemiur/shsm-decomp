#ifndef GAME_CPSYPOINTOFINTEREST_H
#define GAME_CPSYPOINTOFINTEREST_H

// Unit name: the class name string "CPsyPointOfInterest" (0x803EBCF4, 0x803EBD08), which the
// unit's static initialiser hashes into lbl_80529AA0, the class id of the attribute commands of
// fn_801A73A8, and registers with the class registry (lbl_80529AA4).

#include <climax/Camera.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/CPointOfInterest.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>
#include <climax/unk_801F7E80.h>

/*
 * A point of interest whose subject the player is meant to look at (the "psychology" prompt: a
 * "<name>_Prompt" string is shown while the point is in view, centred on the screen and near
 * enough). It follows its frame and tracks how long it has been looked at (vtable lbl_803EBC40,
 * size 0x110). Slots are named after the first implementation; the two overrides of
 * CBaseBehaviour's slots keep CBaseBehaviour's names. Member names invented.
 */
class CPsyPointOfInterest : public CPointOfInterest, public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetName,            // text: the name looked up with fn_8014EC98
        CMD_SetField0xBD,       // integer: zero sets the flag
        CMD_SetField0xA4 = 3,   // float
        CMD_SetField0xA8,       // float
        CMD_SetField0xBE,       // integer: nonzero sets the flag
        CMD_SetMsgA,            // text: message sent when the point is looked at
        CMD_SetMsgB,            // text: message sent when the point is left
        CMD_SetPromptName,      // text: the name of the prompt string
        CMD_SetField0xC2 = 10,  // integer: nonzero sets the flag
        CMD_SetField0xC3,       // integer: nonzero sets the flag
        CMD_SetField0xC4,       // integer: nonzero sets the flag
        CMD_SetMsgC,            // text: the message the point follows the frame of
        CMD_SetField0xC7        // integer: nonzero sets the flag
    };

                    CPsyPointOfInterest( const unk_AttributePacket& packet );
    virtual         ~CPsyPointOfInterest();
    virtual CZone*  fn_800C4E8C();
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual int     fn_800C54EC();
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A7A88();  // override
    virtual void    fn_800C6274( unk_8003C454* who ) {}
    virtual bool    fn_8002CDC8() { return false; }
    virtual CZone*  fn_800C4E94() { return CAttributeHandler::mZone; }
    virtual SClassInfo* fn_800C4E9C() { return CAttributeHandler::mClassInfo; }

    float           fn_800C6D08( const unk_ScreenRect* screen, unk_RwCamera* camera );
    void            fn_800C6E84( unk_80491B38* player );
    void            fn_800C7468( bool entering );

    static CAttributeHandler* fn_800C7688( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPsyPointOfInterest( packet );
    }

    unk_WorldRect   rect;           // 0x78, the frame's position, twice the lengths of its right and up vectors
    unk_801A92A8    msgA;           // 0x8C
    unk_801A92A8    msgB;           // 0x94
    float           field_0x9C;     // how centred the point is on the screen: 1 centred, 0 at the edge or off it
    float           field_0xA0;     // time looked at, weighted by size and centring
    float           field_0xA4;     // scale of the look time (player field_0x1500) that allows the prompt
    float           field_0xA8;     // scale of the look time (player field_0x1504) that counts as looked at
    float           field_0xAC;     // the screen size of the last frame it was looked at
    const char*     name;           // 0xB0, in the attribute packet
    const char*     promptName;     // 0xB4, in the attribute packet; NULL when empty
    u32             promptId;       // 0xB8, localisation id of "<promptName>_Prompt"; 0 when there is no such string
    bool            field_0xBC;
    bool            field_0xBD;
    bool            field_0xBE;
    bool            field_0xBF;
    bool            field_0xC0;
    bool            field_0xC1;
    bool            field_0xC2;
    bool            field_0xC3;
    bool            field_0xC4;
    bool            field_0xC5;
    bool            field_0xC6;
    bool            field_0xC7;
    unk_801A92A8    msgC;           // 0xC8
    unk_RwMatrix    field_0xD0;     // 0xD0
};

extern unk_801A92A8 lbl_8052A820;       // message the point listens to; defined elsewhere, unit not yet reconstructed
extern unk_801A92A8 lbl_80529D00;       // message the point listens to; defined elsewhere, unit not yet reconstructed
extern u32 lbl_80529AA0;                // class-name hash of "CPsyPointOfInterest"
extern unk_801A8F6C lbl_80529AA4;       // type-registry entry of CPsyPointOfInterest
extern u32 lbl_80529AA8;                // time of the last prompt sound
extern bool lbl_80529AAC;               // set: the centring falls with the squared distance from the centre (never set in this unit)

#endif
