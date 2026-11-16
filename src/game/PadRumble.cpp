// unit name unconfirmed: the unit defines the pad rumble messages ("MsgStartBigRumble"...)
// and a debug value table; no string names the file.

#include <game/PadRumble.h>
#include <game/unk_Settings.h>
#include <string.h>

#define MIN( a, b )     ( ( a ) < ( b ) ? ( a ) : ( b ) )
#define MAX( a, b )     ( ( a ) > ( b ) ? ( a ) : ( b ) )

unk_801A92A8 MsgGetPadButtonMask;
unk_801A92A8 lbl_80529108;
unk_801A92A8 MsgStartBigRumble;
unk_801A92A8 MsgStartSmallRumble;
unk_801A92A8 MsgStopBigRumble;
unk_801A92A8 MsgStopSmallRumble;
unk_801A92A8 MsgStopAllRumble;
unk_801A92A8 MsgSuspendRumble;
unk_801A92A8 MsgResumeRumble;

extern const char* const lbl_8052B5E0 = "MsgGetPadButtonMask";
extern const char* const lbl_8052B5E4 = "MsgStartBigRumble";
extern const char* const lbl_8052B5E8 = "MsgStartSmallRumble";
extern const char* const lbl_8052B5EC = "MsgStopBigRumble";
extern const char* const lbl_8052B5F0 = "MsgStopSmallRumble";
extern const char* const lbl_8052B5F4 = "MsgStopAllRumble";
extern const char* const lbl_8052B5F8 = "MsgSuspendRumble";
extern const char* const lbl_8052B5FC = "MsgResumeRumble";

unk_Tweaks lbl_804574F0;

unk_TweakEntry lbl_803DEF20[86] = {
    { TWEAK_FLOAT, &lbl_804574F0.field_0x0 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x4 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x8 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0xC },
    { TWEAK_BOOL , &lbl_804574F0.field_0x10 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x11 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x12 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x13 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x14 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x15 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x16 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x17 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x18 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x19 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x1A },
    { TWEAK_BOOL , &lbl_804574F0.field_0x1B },
    { TWEAK_BOOL , &lbl_804574F0.field_0x1C },
    { TWEAK_BOOL , &lbl_804574F0.field_0x1D },
    { TWEAK_BOOL , &lbl_804574F0.field_0x1E },
    { TWEAK_BOOL , &lbl_804574F0.field_0x1F },
    { TWEAK_BOOL , &lbl_804574F0.field_0x20 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x21 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x22 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x23 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x24 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x28 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x2C },
    { TWEAK_BOOL , &lbl_804574F0.field_0x30 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x31 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x32 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x33 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x34 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x35 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x36 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x37 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x38 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x3C },
    { TWEAK_BOOL , &lbl_804574F0.field_0x40 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x41 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x42 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x43 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x44 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x45 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x46 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x47 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x48 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x49 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x4C },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x50 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x54 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x55 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x56 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x57 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x58 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x59 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x5C },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x60 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x64 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x65 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x68 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x6C },
    { TWEAK_BOOL , &lbl_804574F0.field_0x70 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x71 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x72 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x73 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x74 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x75 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x76 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x77 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x78 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x79 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x7A },
    { TWEAK_BOOL , &lbl_804574F0.field_0x7B },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x7C },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x80 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x84 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x85 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x86 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x87 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x88 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x89 },
    { TWEAK_BOOL , &lbl_804574F0.field_0x8A },
    { TWEAK_BOOL , &lbl_804574F0.field_0x8B },
    { TWEAK_BOOL , &lbl_804574F0.field_0x8C },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x90 },
    { TWEAK_FLOAT, &lbl_804574F0.field_0x94 },
};

static bool lbl_80529148;

/*
====================
unk_Tweaks::unk_Tweaks

Clears the debug values. As in retail, it leaves field_0x18, field_0x40, field_0x89 to
field_0x8C, field_0x90 and field_0x94 unset (they stay zero as .bss).
====================
*/
inline unk_Tweaks::unk_Tweaks() {
    field_0x0 = 0.0f;
    field_0x4 = 0.0f;
    field_0x8 = 0.0f;
    field_0xC = 0.0f;
    field_0x10 = false;
    field_0x11 = false;
    field_0x12 = false;
    field_0x13 = false;
    field_0x14 = false;
    field_0x15 = false;
    field_0x16 = false;
    field_0x17 = false;
    field_0x19 = false;
    field_0x1A = false;
    field_0x1B = false;
    field_0x1C = false;
    field_0x1D = false;
    field_0x1E = false;
    field_0x1F = false;
    field_0x20 = false;
    field_0x21 = false;
    field_0x22 = false;
    field_0x23 = false;
    field_0x24 = false;
    field_0x28 = 0.0f;
    field_0x2C = 0.0f;
    field_0x30 = false;
    field_0x31 = false;
    field_0x32 = false;
    field_0x33 = false;
    field_0x34 = false;
    field_0x35 = false;
    field_0x36 = false;
    field_0x37 = false;
    field_0x38 = 0.0f;
    field_0x3C = 0.0f;
    field_0x41 = false;
    field_0x42 = false;
    field_0x43 = false;
    field_0x44 = false;
    field_0x45 = false;
    field_0x46 = false;
    field_0x47 = false;
    field_0x48 = false;
    field_0x49 = false;
    field_0x4C = 0.0f;
    field_0x50 = 0.0f;
    field_0x54 = false;
    field_0x55 = false;
    field_0x56 = false;
    field_0x57 = false;
    field_0x58 = false;
    field_0x59 = false;
    field_0x5C = 0.0f;
    field_0x60 = 0.0f;
    field_0x64 = false;
    field_0x65 = false;
    field_0x68 = 0.0f;
    field_0x6C = 0.0f;
    field_0x70 = false;
    field_0x71 = false;
    field_0x72 = false;
    field_0x73 = false;
    field_0x74 = false;
    field_0x75 = false;
    field_0x76 = false;
    field_0x77 = false;
    field_0x78 = false;
    field_0x79 = false;
    field_0x7A = false;
    field_0x7B = false;
    field_0x7C = 0.0f;
    field_0x80 = 0.0f;
    field_0x84 = false;
    field_0x85 = false;
    field_0x86 = false;
    field_0x87 = false;
    field_0x88 = false;
}

/*
====================
fn_800067A0

Clears every debug value.
====================
*/
// 0x800067A0
void fn_800067A0() {
    memset( &lbl_804574F0, 0, sizeof( lbl_804574F0 ) );
}

/*
====================
fn_800067B4
====================
*/
// 0x800067B4
unk_Tweaks* fn_800067B4() {
    return &lbl_804574F0;
}

/*
====================
fn_800067C0
====================
*/
// 0x800067C0
unk_TweakEntry* fn_800067C0() {
    return lbl_803DEF20;
}

/*
====================
unk_Ramp::fn_800067CC

Advances the ramp by dt seconds; clears it once its lifetime has passed.
====================
*/
// 0x800067CC
void unk_Ramp::fn_800067CC( float dt ) {
    elapsed += dt;
    if ( lifetime >= 0.0f && elapsed >= lifetime ) {
        memset( this, 0, sizeof( *this ) );
        return;
    }
    if ( value != target ) {
        float step = dt * rate;
        if ( step < 0.0f ) {
            value = MAX( value + step, target );
        } else {
            value = MIN( value + step, target );
        }
    }
}

/*
====================
unk_Ramp::fn_80006860

Starts a ramp to target over rampTime seconds, lasting lifetime seconds.
====================
*/
// 0x80006860
void unk_Ramp::fn_80006860( float target, float lifetime, float rampTime ) {
    if ( fn_800C770C( 12 ) || target == 0.0f ) {
        this->target = MIN( 1.0f, MAX( 0.0f, target ) );
        this->lifetime = lifetime;
        elapsed = 0.0f;
        if ( rampTime > 0.0f ) {
            rate = ( this->target - value ) / rampTime;
        } else {
            value = this->target;
            rate = 0.0f;
        }
    }
}

/*
====================
unk_Ramp::fn_80006954

Saves the ramp and silences it.
====================
*/
// 0x80006954
void unk_Ramp::fn_80006954() {
    if ( !suspended ) {
        savedTarget = target;
        savedValue = value;
        savedLifetime = lifetime;
        savedRate = rate;
        savedElapsed = elapsed;
        fn_80006860( 0.0f, 0.0f, 0.0f );
        suspended = true;
    }
}

/*
====================
unk_Ramp::fn_800069D8

Restores the ramp saved by fn_80006954.
====================
*/
// 0x800069D8
void unk_Ramp::fn_800069D8() {
    if ( suspended ) {
        target = savedTarget;
        value = savedValue;
        lifetime = savedLifetime;
        rate = savedRate;
        elapsed = savedElapsed;
        suspended = false;
    }
}

/*
====================
fn_80006A18
====================
*/
// 0x80006A18
void fn_80006A18( bool on ) {
    lbl_80529148 = on;
}

/*
====================
fn_80006A20
====================
*/
// 0x80006A20
bool fn_80006A20() {
    return lbl_80529148;
}
