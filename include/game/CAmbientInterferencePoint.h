#ifndef GAME_CAMBIENTINTERFERENCEPOINT_H
#define GAME_CAMBIENTINTERFERENCEPOINT_H

#include <climax/Frame.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Display.h>
#include <game/Messages.h>
#include <game/SaveData.h>
#include <game/unk_80491B38.h>

/*
 * The interference sound that a CAmbientInterferencePoint fades in near the player
 * (0x80157D08-0x80158130, unit not yet reconstructed). Size 0x58. Name invented.
 */
class unk_80157E60 {
public:
                unk_80157E60();
    float       fn_80157D08( float a, float b );
    void        fn_80157F7C( float dt, float level );

    u8          field_0x0[0x48];
    float       field_0x48;
    u8          field_0x4C[0xC];
};

// defined in 0x801580A8, unit not yet reconstructed: sets the interference level
void fn_801580A8( float level );

/* The object behind lbl_8052A10C; only the field written here is known. Name invented. */
struct unk_8052A10C {
    u8      field_0x0[0x14];
    float   field_0x14;
};

// defined elsewhere, unit not yet reconstructed
extern unk_8052A10C* lbl_8052A10C;

/* The camera record that answers lbl_8052A120; only the view direction is known. Name invented. */
struct unk_CameraInfo {
    u8          field_0x0[0x20];
    unk_RwV3d   direction;      // 0x20
};

// Messages; defined elsewhere, units not yet reconstructed.
extern unk_801A92A8 lbl_8052A0B0;  // sent with &field_0x54 when the point starts listening
extern unk_801A92A8 lbl_8052A0B8;  // sent with &field_0x54 when the point stops or fires
extern unk_801A92A8 lbl_8052A120;  // a query answered with the camera (unk_CameraInfo)

extern const float lbl_80530080;    // .sbss2; owner unknown

// defined in 0x80070284, unit not yet reconstructed
bool    fn_80070284( CBaseBehaviour* target );

extern const char* lbl_80527480;    // "AmbientInterferenceData"
extern const char* lbl_80527484;    // "AmbientInterferenceID"
extern const char* lbl_80527488;    // "SaveData"
extern bool lbl_8052748C;

/*
 * An ambient behaviour placed in the world (vtable __vt__25CAmbientInterferencePoint, registered as "CAmbientInterferencePoint").
 * When the player comes within range and looks towards it, it fades in an interference sound and fires.
 */
class CAmbientInterferencePoint : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetFieldMsg60,
        CMD_SetField38,
        CMD_SetField3C,
        CMD_SetField40,
        CMD_SetField44,
        CMD_SetFieldMsg68,
        CMD_SetField74,
        CMD_SetField78,
        CMD_SetFieldD5,
        CMD_SetFieldMsgD8,
        CMD_SetFieldMsgE0,
        CMD_SetFieldD4
    };

                    CAmbientInterferencePoint( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~CAmbientInterferencePoint();
    virtual void    fn_801A7A88();
    virtual void    fn_8000CFCC();
    virtual int     fn_800C2790();

    static CAttributeHandler* fn_800C2BF0( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CAmbientInterferencePoint( packet );
    }

    int             fn_800C2304( CBaseBehaviour* target );
    void            fn_800C25EC( float dt );
    void            fn_800C2A0C( const char* text, bool arg );

    void            StartListening();   // name invented
    void            StopListening();    // name invented

    bool        field_0x34;     // fired
    bool        field_0x35;
    bool        field_0x36;     // armed
    float       field_0x38;     // outer range
    float       field_0x3C;     // inner range
    float       field_0x40;     // view angle, degrees
    float       field_0x44;     // vertical distance scale
    float       field_0x48;     // distance to the player
    float       field_0x4C;     // angle to the player, degrees
    bool        field_0x50;     // sound playing
    u32         field_0x54;     // payload of lbl_8052A0B0 and lbl_8052A0B8
    float       field_0x58;
    float       field_0x5C;
    unk_801A92A8 field_0x60;
    unk_801A92A8 field_0x68;
    u32         field_0x70;
    char*       field_0x74;
    char*       field_0x78;
    unk_80157E60 field_0x7C;
    bool        field_0xD4;
    bool        field_0xD5;     // listening
    unk_801A92A8 field_0xD8;
    unk_801A92A8 field_0xE0;
};

extern u32          lbl_80529A40;   // class id of "CAmbientInterferencePoint"
extern unk_801A8F6C lbl_80529A44;   // registration of "CAmbientInterferencePoint"

#endif
