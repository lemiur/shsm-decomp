#ifndef GAME_CICETRANSITION_H
#define GAME_CICETRANSITION_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <climax/Frame.h>
#include <game/CBaseBehaviour.h>
#include <revolution/gx/GXTransform.h>
#include <revolution/mtx.h>

/*
 * The ice transition behaviour. Its class name is the type-registry string "cIceTransition" in
 * the registration of the static initialiser fn_80088DB0; its factory is MakeNew in
 * AI::CAiWaterPushBlock. Size 0xA4, vtable lbl_803E82D8. Member names invented.
 */
/*
 * One entry of the table the transition writes when it is switched on or off: the target word, the
 * value it takes when off and the value it takes when on. Names invented.
 */
struct s_IceSwitch {
    u32*            target;
    u32             offValue;
    u32             onValue;
};

class cIceTransition : public CBaseBehaviour {
public:
    /* Attribute commands, in the order of their ids. Names invented from the fields they set. */
    enum {
        CMD_SetFieldMsg34,
        CMD_SetField70,
        CMD_SetField6C,
        CMD_SetField54,
        CMD_SetField74,
        CMD_SetField78,
        CMD_SetFieldMsg3C,
        CMD_SetFieldMsg44,
        CMD_SetField8C,
        CMD_SetField90,
        CMD_SetFieldMsg4C
    };

                    cIceTransition( const unk_AttributePacket& packet );
    virtual         ~cIceTransition();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_801A7A88();

    void            fn_800885C8( bool enable );
    void            fn_8008863C( float frameTime );
    cIceTransition* fn_800886E8();
    void            fn_80088748( float frameTime );     // the argument is unused by the body

    unk_801A92A8    field_0x34;
    unk_801A92A8    field_0x3C;
    unk_801A92A8    field_0x44;
    unk_801A92A8    field_0x4C;
    const char*     field_0x54;     // the texture name fn_801A7A88 looks up
    RwUInt8*        field_0x58[4];  // buffers of 0x800 bytes from RwMalloc, freed by the destructor; each
                                    // becomes the palette of the texture for one frame of the table
    f32             field_0x68;     // the transition's progress, 0.0 at the start
    f32             field_0x6C;     // 1.0 at the start
    f32             field_0x70;
    f32             field_0x74;
    f32             field_0x78;
    bool            field_0x7C;
    bool            field_0x7D;     // true at the start
    RwTexture*      field_0x80;
    f32             field_0x84;     // 1.0 at the start
    bool            field_0x88;
    f32             field_0x8C;     // 0.5 at the start
    f32             field_0x90;     // 3.0 at the start
    u32             field_0x94;
    s32             field_0x98;     // entries in field_0xA0
    bool            field_0x9C;
    s_IceSwitch*    field_0xA0;     // array of field_0x98 entries, released with delete[]
};

/*
 * Builds the palette of the texture's raster for the given fade level and makes it the raster's
 * TLUT. A free function: the caller passes the texture, not a transition. Names invented.
 */
void fn_800888F0( RwTexture* texture, s32 level );

// Declared in rwsdk/world/climaxplatform.h, which cannot be included beside climax/Frame.h (the
// rwCOMBINEREPLACE enumerator is declared in both).
void RwClimaxSetTexture( RwTexture* texture, RwInt32 index );
// Declared in rwsdk/world/climaxplatform.h (see above): uploads the matrix as a GX texture matrix.
extern "C" void RwClimaxRVLConvertMatrix( const unk_RwMatrix* rwMatrix, Mtx mtx );

// defined in auto_03 (asm, 0x802B3520): stores value in slot of a table of words
extern "C" void fn_802B3520( s32 slot, u32 value );

extern u32 lbl_805296A0;                // class-name hash of "cIceTransition", defined in game/cIceTransition.cpp
extern cIceTransition* lbl_805296B0;    // the running transition, defined in game/cIceTransition.cpp
extern s32 lbl_805296B4;                // the table index of the last frame; name invented, defined in game/cIceTransition.cpp
extern Mtx lbl_8046A568;                // the texture matrix the transition loads; name invented

extern unk_801A92A8 lbl_8052A788;   // defined in climax/EventHandler.cpp
extern unk_801A92A8 lbl_8052A858;   // defined in climax/EventHandler.cpp

/*
 * The trigger behaviour that drives the transition, registered as "cIceTransitionTrigger" with
 * size 0x40 and factory fn_80088EA0. Its message handler (retail 0x80088B68) and attribute handler
 * (retail 0x80088D18) are not reconstructed yet. Member names invented.
 */
class cIceTransitionTrigger : public CBaseBehaviour {
public:
                    cIceTransitionTrigger() {
                        fn_801A9DE8( lbl_8052A788 );
                        field_0x3C = 99999.0f;
                    }
    virtual         ~cIceTransitionTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_80088EA0( const unk_AttributePacket& packet );

    unk_801A92A8    field_0x34;
    f32             field_0x3C;     // 99999.0 at the start
};

#endif
