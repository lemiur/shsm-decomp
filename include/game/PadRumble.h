#ifndef GAME_PADRUMBLE_H
#define GAME_PADRUMBLE_H

#include <climax/unk_801A92A8.h>
#include <string.h>

/*
 * Messages of the pad rumble interface, registered by name in fn_80006C7C's unit.
 * Each name string has a matching const pointer (lbl_8052B5E0...).
 */
extern unk_801A92A8 MsgGetPadButtonMask;
extern unk_801A92A8 lbl_80529108;   // constructed but never registered
extern unk_801A92A8 MsgStartBigRumble;
extern unk_801A92A8 MsgStartSmallRumble;
extern unk_801A92A8 MsgStopBigRumble;
extern unk_801A92A8 MsgStopSmallRumble;
extern unk_801A92A8 MsgStopAllRumble;
extern unk_801A92A8 MsgSuspendRumble;
extern unk_801A92A8 MsgResumeRumble;

extern const char* const lbl_8052B5E0;  // "MsgGetPadButtonMask"
extern const char* const lbl_8052B5E4;  // "MsgStartBigRumble"
extern const char* const lbl_8052B5E8;  // "MsgStartSmallRumble"
extern const char* const lbl_8052B5EC;  // "MsgStopBigRumble"
extern const char* const lbl_8052B5F0;  // "MsgStopSmallRumble"
extern const char* const lbl_8052B5F4;  // "MsgStopAllRumble"
extern const char* const lbl_8052B5F8;  // "MsgSuspendRumble"
extern const char* const lbl_8052B5FC;  // "MsgResumeRumble"

/* The payload of the start and stop rumble messages, named by their payload type string. Member names invented. */
struct RumbleStartData {
    int     channel;    // 0x0, the pad
    float   level;      // 0x4
    float   lifetime;   // 0x8
    float   rampTime;   // 0xC
};

/*
 * A value that ramps towards a target at a fixed rate and expires after a lifetime;
 * Suspend saves the state and silences it until Resume. Name and member names invented.
 */
class unk_Ramp {
public:
            unk_Ramp() { memset( this, 0, sizeof( *this ) ); }

    /* Name invented. */
    float   GetValue() const { return value; }

    void    fn_800067CC( float dt );
    void    fn_80006860( float target, float lifetime, float rampTime );
    void    fn_80006954();
    void    fn_800069D8();

    float   target;         // 0x00, clamped to [0, 1]
    float   value;          // 0x04
    float   lifetime;       // 0x08, seconds; negative never expires
    float   rate;           // 0x0C, change of value per second
    float   elapsed;        // 0x10, seconds since the last Set
    float   savedTarget;    // 0x14, the five fields above while suspended
    float   savedValue;     // 0x18
    float   savedLifetime;  // 0x1C
    float   savedRate;      // 0x20
    float   savedElapsed;   // 0x24
    int     suspended;      // 0x28
};

/* Kinds of entry in the debug value table lbl_803DEF20. Names invented. */
enum {
    TWEAK_FLOAT,
    TWEAK_BOOL
};

/* One entry of the debug value table: its kind and the value it edits. Name invented. */
struct unk_TweakEntry {
    int     type;
    void*   value;
};

/* Debug values: floats and switches, each listed in lbl_803DEF20. Name invented; members unknown. */
struct unk_Tweaks {
    unk_Tweaks();

    float field_0x0;
    float field_0x4;
    float field_0x8;
    float field_0xC;
    bool  field_0x10;
    bool  field_0x11;
    bool  field_0x12;
    bool  field_0x13;
    bool  field_0x14;
    bool  field_0x15;
    bool  field_0x16;
    bool  field_0x17;
    bool  field_0x18;
    bool  field_0x19;
    bool  field_0x1A;
    bool  field_0x1B;
    bool  field_0x1C;
    bool  field_0x1D;
    bool  field_0x1E;
    bool  field_0x1F;
    bool  field_0x20;
    bool  field_0x21;
    bool  field_0x22;
    bool  field_0x23;
    bool  field_0x24;
    float field_0x28;
    float field_0x2C;
    bool  field_0x30;
    bool  field_0x31;
    bool  field_0x32;
    bool  field_0x33;
    bool  field_0x34;
    bool  field_0x35;
    bool  field_0x36;
    bool  field_0x37;
    float field_0x38;
    float field_0x3C;
    bool  field_0x40;
    bool  field_0x41;
    bool  field_0x42;
    bool  field_0x43;
    bool  field_0x44;
    bool  field_0x45;
    bool  field_0x46;
    bool  field_0x47;
    bool  field_0x48;
    bool  field_0x49;
    float field_0x4C;
    float field_0x50;
    bool  field_0x54;
    bool  field_0x55;
    bool  field_0x56;
    bool  field_0x57;
    bool  field_0x58;
    bool  field_0x59;
    float field_0x5C;
    float field_0x60;
    bool  field_0x64;
    bool  field_0x65;
    float field_0x68;
    float field_0x6C;
    bool  field_0x70;
    bool  field_0x71;
    bool  field_0x72;
    bool  field_0x73;
    bool  field_0x74;
    bool  field_0x75;
    bool  field_0x76;
    bool  field_0x77;
    bool  field_0x78;
    bool  field_0x79;
    bool  field_0x7A;
    bool  field_0x7B;
    float field_0x7C;
    float field_0x80;
    bool  field_0x84;
    bool  field_0x85;
    bool  field_0x86;
    bool  field_0x87;
    bool  field_0x88;
    bool  field_0x89;
    bool  field_0x8A;
    bool  field_0x8B;
    bool  field_0x8C;
    float field_0x90;
    float field_0x94;
};

extern unk_Tweaks lbl_804574F0;
extern unk_TweakEntry lbl_803DEF20[86];

void        fn_800067A0();
unk_Tweaks* fn_800067B4();
unk_TweakEntry* fn_800067C0();
void        fn_80006A18( bool on );
bool        fn_80006A20();

#endif
