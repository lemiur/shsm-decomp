#ifndef GAME_CFOGOVERIDE_H
#define GAME_CFOGOVERIDE_H

#include <revolution/types.h>
#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <climax/unk_801A92A8.h>
#include <game/CBaseBehaviour.h>

/*
 * One fog setting, linked into the list headed by lbl_80529658 through its first word. The record
 * constructor 0x80085CBC pushes it onto the list. Name invented (placeholder from that address).
 * cFogOveride embeds one at 0x5C: its list node, and its 0x80 and 0x84 are the setting's active
 * flag and id.
 */
struct unk_80085CBC {
    unk_80085CBC*   next;           // 0x00
    float           field_0x04;     // 0x04, red, 0.0 by default
    float           field_0x08;     // 0x08, green, 0.0 by default
    float           field_0x0C;     // 0x0C, blue, 0.0 by default
    float           field_0x10;     // 0x10, not set by the constructors
    float           field_0x14;     // 0x14, 0.0 by default
    float           field_0x18;     // 0x18, 50.0 by default
    float           field_0x1C;     // 0x1C, 0.3 by default
    float           field_0x20;     // 0x20, 0.1 by default: the weight the search compares
    u8              field_0x24;     // 0x24, nonzero: the setting is active
    u32             field_0x28;     // 0x28, the id the search matches against the key
};

/*
 * The fog state block lbl_8046A480 (0x40 bytes). Name invented. Only the offsets the update and
 * the search read and write are named.
 */
struct unk_FogState {
    u8              field_0x00[0x10];
    float           field_0x10;
    float           field_0x14;
    float           field_0x18;
    float           field_0x1C;
    float           field_0x20;
    float           field_0x24;
    float           field_0x28;
    float           field_0x2C;
    float           field_0x30;
    float           field_0x34;
    float           field_0x38;
    float           field_0x3C;
};

/*
 * A fog override: a behaviour that, while its message id (field_0x34 turns it on, field_0x3C
 * turns it off), takes its setting as the fog. Vtable lbl_803E7E68, size 0x88. The name comes from
 * the class-registry string "cFogOveride" (0x803E7EC4 and 0x803E7ED0). The member names are invented
 * and the member functions are free functions below (open work).
 */
struct cFogOveride : public CBaseBehaviour {
    cFogOveride();
    unk_801A92A8    field_0x34;     // 0x34, the message id that turns the override on
    unk_801A92A8    field_0x3C;     // 0x3C, the message id that turns it off
    u8              field_0x44[4];  // 0x44, the colour bytes of the packet's colour command
    float           field_0x48;     // 0x48
    float           field_0x4C;     // 0x4C
    float           field_0x50;     // 0x50
    float           field_0x54;     // 0x54, 0.0 by default
    float           field_0x58;     // 0x58
    unk_80085CBC    setting;        // 0x5C, the list node and the fog setting
};

extern u32 lbl_80529650;            // class-name hash of "cFogOveride"
extern unk_801A8F6C lbl_80529654;   // type-registry entry of cFogOveride
extern unk_80085CBC* lbl_80529658;  // head of the list of fog settings
extern unk_80085CBC* lbl_8052965C;  // the setting the fog currently uses
extern u32 lbl_80529660;            // the key the search matches: the player's id, or 0
extern u8 lbl_80529664[3];          // the fog colour bytes, written by the update
extern float lbl_80529668;          // the blend from the old setting to the new one
extern float lbl_8052966C;          // the time the blend takes
extern unk_FogState lbl_8046A480;   // the fog state block
extern float lbl_80527238;          // 50.0f, the fog start of the setting the update blends to
extern float lbl_8052723C;          // 0.3f, the fog end of that setting
extern float lbl_80527240;          // 50.0f, the start the blend comes from
extern float lbl_80527244;          // 0.3f, the end the blend comes from
extern float lbl_80527248;          // 50.0f, the start of the current setting
extern float lbl_8052724C;          // 0.3f, the end of the current setting

/* Free-function forms of this unit's member functions; see the class comment. */
void    fn_80085CBC( unk_80085CBC* self );
unk_80085CBC* fn_80085D30( unk_80085CBC* self, int deleteFlag );
void    fn_80085DB8( float dt );
void    fn_80085FB8();
void    fn_80086118( cFogOveride* self, int deleteFlag );
void    fn_8008620C( cFogOveride* self, unk_Msg& msg );
void    fn_80086294( cFogOveride* self, const unk_AttributePacket& packet );
void    fn_80086498( cFogOveride* self );
CAttributeHandler* fn_80086504( const unk_AttributePacket& packet );
void    fn_800865CC( cFogOveride* self, unk_Msg& msg );
void    fn_800865D4( cFogOveride* self, int deleteFlag );

#endif
