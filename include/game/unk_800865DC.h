#ifndef GAME_UNK_800865DC_H
#define GAME_UNK_800865DC_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <game/CBaseBehaviour.h>

/*
 * A behaviour (constructor 0x800865DC, vtable __vt__12unk_800865DC, size 0xA8). Every instance is linked
 * into the list headed by lbl_80529670 (next at 0xA4). Class name invented after its constructor.
 */
class unk_800865DC : public CBaseBehaviour {
public:
    /* Attribute commands, in the order of the switch's jump table. Names invented. */
    enum {
        CMD_SetField34,
        CMD_SetField38,
        CMD_SetField3C,
        CMD_SetField40,
        CMD_SetField44,
        CMD_SetField48,
        CMD_SetField4C,
        CMD_SetField50,
        CMD_SetField54,
        CMD_SetField58,
        CMD_SetColor,
        CMD_SetField68,
        CMD_SetField6C,
        CMD_SetField70,
        CMD_SetField78,
        CMD_SetField7C,
        CMD_SetField80,
        CMD_SetState,
        CMD_SetFieldMsg84,
        CMD_SetFieldMsg8C,
        CMD_SetField94,
        CMD_SetField98
    };

                    // the packet is not read here; the registry factory passes it on
                    unk_800865DC( const unk_AttributePacket& packet );
    // registry factory of the class (CBloomCfg, see unk_800865DC.cpp)
    static CAttributeHandler* fn_80087564( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~unk_800865DC();
                    // a timed falloff of the frame against the target matrix (its own file comment)
    float           fn_80086B0C( float dt, const unk_RwMatrix* target );

    float           field_0x34;
    int             field_0x38;
    float           field_0x3C;
    float           field_0x40;
    int             field_0x44;
    float           field_0x48;
    float           field_0x4C;
    float           field_0x50;
    float           field_0x54;
    int             field_0x58;
    float           field_0x5C;     // colour, blue (the low byte of the command's word)
    float           field_0x60;     // colour, green
    float           field_0x64;     // colour, red
    float           field_0x68;
    int             field_0x6C;
    float           field_0x70;
    float           field_0x74;     // field_0x70 squared
    float           field_0x78;
    int             field_0x7C;
    float           field_0x80;
    unk_801A92A8    field_0x84;     // message; the handler of field_0x84 sets the state
    unk_801A92A8    field_0x8C;     // message
    int             field_0x94;
    float           field_0x98;
    int             field_0x9C;     // state: 0 idle, 1 and 2 armed, 3 done
    float           field_0xA0;     // timer
    unk_800865DC*   next;           // 0xA4, the instance list
};

/* Behaviour B (constructor 0x80086F9C, vtable __vt__12unk_80086F9C, size 0x60); see unk_800865DC.cpp. */
class unk_80086F9C : public CBaseBehaviour {
public:
    /* Attribute commands of unk_80086F9C, by id (names invented). */
    enum {
        CMD_SetField34,
        CMD_SetState,
        CMD_SetFieldMsg38,
        CMD_SetField4C,
        CMD_SetFieldMsg40,
        CMD_SetField50,
        CMD_SetField48
    };

                    // the packet is not read here; the registry factory passes it on
                    unk_80086F9C( const unk_AttributePacket& packet );
    // registry factory of the class (CMotionBlurCfg, see unk_800865DC.cpp)
    static CAttributeHandler* fn_800875AC( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~unk_80086F9C();
    // the per-frame envelope of the instance, one step of dt; returns its factor times field_0x34
    float           fn_80087314( float dt );

    float           field_0x34;
    unk_801A92A8    field_0x38;     // message
    unk_801A92A8    field_0x40;     // message
    int             field_0x48;
    float           field_0x4C;
    float           field_0x50;
    int             field_0x54;     // state
    float           field_0x58;     // timer
    unk_80086F9C*   next;           // 0x5C, the instance list
};

// the timed-falloff list walker: moves the lerps below by each instance's falloff at target
void fn_80086D7C( float dt, const unk_RwMatrix* target );

// advances every instance of unk_80086F9C by dt and returns the sum of their factors
float fn_80087448( float dt );

// defined in this unit: the head of each instance list and the flag that fn_801A73A8 sets
extern unk_800865DC* lbl_80529670;
extern u32 lbl_80529674;        // the class id of unk_800865DC's attribute commands
extern unk_801A8F6C lbl_80529678;   // registry entry of unk_800865DC
extern unk_80086F9C* lbl_8052967C;
extern u32 lbl_80529680;        // the class id of unk_80086F9C's attribute commands
extern unk_801A8F6C lbl_80529684;   // registry entry of unk_80086F9C
extern bool lbl_80527250;
// the colour and intensity lerps of fn_80086D7C (defined in unk_800865DC.cpp)
extern float lbl_80528818;
extern float lbl_8052881C;
extern float lbl_80528820;
extern float lbl_80528824;
extern float lbl_80528828;
extern float lbl_8052882C;
extern float lbl_80528830;
extern float lbl_80528834;
extern float lbl_80528838;
extern float lbl_8052A680;

#endif
