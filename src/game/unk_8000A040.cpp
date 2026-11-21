// unit name unconfirmed: the unit's class receives the pad rumble messages
// ("MsgStartBigRumble"...) and sets KPAD parameters; no string names the class or the file.

#include <game/unk_8000A040.h>
#include <game/DisplayHeight.h>
#include <game/PadRumble.h>
#include <game/unk_80491B38.h>
#include <math.h>

unk_8000A040* lbl_80529160;


unk_PadChannel lbl_804579C0[4];
unk_PointerTuning lbl_8045A680;

int lbl_803DF270[41][12] = {
    { 1, 85, TWEAK_END },
    { 0, 84, TWEAK_END },
    { 18, 81, TWEAK_END },
    { 19, 83, TWEAK_END },
    { 20, 80, TWEAK_END },
    { 21, 82, TWEAK_END },
    { 12, 18, 29, 44, 41, TWEAK_END },
    { 19, 30, 5, 43, 42, TWEAK_END },
    { 10, 20, 31, 45, TWEAK_END },
    { 11, 21, 32, 46, TWEAK_END },
    { 7, 22, 33, 38, 51, 53, 75, TWEAK_END },
    { 4, 58, 13, 52, 54, 76, TWEAK_END },
    { 15, 77, 57, TWEAK_END },
    { 6, 39, 16, 14, TWEAK_END },
    { 8, 40, 78, TWEAK_END },
    { 9, 34, 23, TWEAK_END },
    { TWEAK_END },
    { 37, 17, TWEAK_END },
    { 24, TWEAK_END },
    { 25, TWEAK_END },
    { 26, TWEAK_END },
    { 27, TWEAK_END },
    { 3, TWEAK_END },
    { 2, TWEAK_END },
    { 28, TWEAK_END },
    { 35, TWEAK_END },
    { 36, TWEAK_END },
    { 61, TWEAK_END },
    { 62, TWEAK_END },
    { 63, TWEAK_END },
    { 64, TWEAK_END },
    { 65, TWEAK_END },
    { 66, TWEAK_END },
    { 67, TWEAK_END },
    { 68, TWEAK_END },
    { 69, 79, TWEAK_END },
    { 70, 79, TWEAK_END },
    { 71, 79, TWEAK_END },
    { 72, 79, TWEAK_END },
    { 73, 55, TWEAK_END },
    { 74, 56, TWEAK_END },
};

/*
====================
HasMotion

The sample carries the acceleration of the remote (source 0) or of the nunchuk (source 1). Name invented.
====================
*/
static inline bool HasMotion( const KPADStatus& status, int source ) {
    if ( source == 0 ) {
        if ( status.data_format == WPAD_FMT_CORE_BTN_ACC || status.data_format == WPAD_FMT_CORE_BTN_ACC_DPD
             || status.data_format == WPAD_FMT_FS_BTN_ACC || status.data_format == WPAD_FMT_FS_BTN_ACC_DPD ) {
            return true;
        }
    } else if ( status.data_format == WPAD_FMT_FS_BTN_ACC || status.data_format == WPAD_FMT_FS_BTN_ACC_DPD ) {
        return true;
    }
    return false;
}

/*
====================
V3dAdd

dst = a + b; paired singles. Name invented.
====================
*/
static inline void V3dAdd( register unk_Vec3* dst, register const unk_Vec3* a, register const unk_Vec3* b ) {
    asm {
        psq_l       fp2, 0(a), 0, 0
        psq_l       fp3, 8(a), 1, 0
        psq_l       fp1, 8(b), 1, 0
        ps_add      fp3, fp3, fp1
        psq_l       fp0, 0(b), 0, 0
        psq_st      fp3, 8(dst), 1, 0
        ps_add      fp2, fp2, fp0
        psq_st      fp2, 0(dst), 0, 0
    }
}

/*
====================
V3dDivide

dst = v / d; paired singles. Name invented.
====================
*/
static inline void V3dDivide( register unk_Vec3* dst, register const unk_Vec3* v, register float d ) {
    asm {
        psq_l       fp0, 0(v), 0, 0
        psq_l       fp1, 8(v), 1, 0
        ps_merge00  fp2, d, d
        ps_div      fp3, fp0, fp2
        ps_div      fp4, fp1, fp2
        psq_st      fp3, 0(dst), 0, 0
        psq_st      fp4, 8(dst), 1, 0
    }
}

/* The samples a queue keeps. */
const int lbl_8052B6F8 = 64;

/* Clamps value to [low, high]. Name invented. */
template< class T >
inline const T& unk_Clamp( const T& value, const T& low, const T& high ) {
    return value > high ? high : ( value < low ? low : value );
}

/*
====================
fn_8000A028

Empty in release: every caller passes a flag (true or false); presumably a debug setter whose body is
compiled out. bool provisional: the callers load 0 or 1, which an int parameter would also take.
====================
*/
// 0x8000A028
void fn_8000A028( bool value ) {
}

/*
====================
fn_8000A02C
====================
*/
// 0x8000A02C
bool fn_8000A02C() {
    return true;
}

/*
====================
fn_8000A034

Sets the second flag at the end of the manager (field_0x5139).
====================
*/
// 0x8000A034
void fn_8000A034( bool value ) {
    lbl_80529160->field_0x5139 = value;
}

/*
====================
unk_8000A040::unk_8000A040
====================
*/
// 0x8000A040
unk_8000A040::unk_8000A040() {
    fn_801A9DE8( lbl_8052A780, NULL, 0x8000 );
    fn_801A9DE8( lbl_8052A798, NULL, 0x8000 );

    fn_801A93B4( MsgStartBigRumble, lbl_8052B5E4, "RumbleStartData*" );
    fn_801A93B4( MsgStartSmallRumble, lbl_8052B5E8, "RumbleStartData*" );
    fn_801A93B4( MsgStopBigRumble, lbl_8052B5EC, "RumbleStartData*" );
    fn_801A93B4( MsgStopSmallRumble, lbl_8052B5F0, "RumbleStartData*" );
    fn_801A93B4( MsgStopAllRumble, lbl_8052B5F4, "" );
    fn_801A93B4( MsgSuspendRumble, lbl_8052B5F8, "" );
    fn_801A93B4( MsgResumeRumble, lbl_8052B5FC, "" );

    fn_801A9DE8( MsgStartBigRumble, NULL, 0x8000 );
    fn_801A9DE8( MsgStartSmallRumble, NULL, 0x8000 );
    fn_801A9DE8( MsgStopBigRumble, NULL, 0x8000 );
    fn_801A9DE8( MsgStopSmallRumble, NULL, 0x8000 );
    fn_801A9DE8( MsgStopAllRumble, NULL, 0x8000 );
    fn_801A9DE8( MsgSuspendRumble, NULL, 0x8000 );
    fn_801A9DE8( MsgResumeRumble, NULL, 0x8000 );

    fn_801A93B4( MsgGetPadButtonMask, lbl_8052B5E0, "RwUInt32" );
    fn_801A9DE8( MsgGetPadButtonMask, "RwUInt32", 0x8000 );

    field_0x8 = 0;
    lbl_80529160 = this;
    field_0x512C = 3.0f;
    field_0x5130 = 3.0f;
    field_0x5134 = 0.0f;
    field_0x5138 = false;
    field_0xA0 = false;
    field_0xC.field_0x0.Reset();
    field_0xC.field_0x34.Reset();
    unk_SetPosParam( 0.02f, 0.75f );
    field_0x5139 = false;
}

/*
====================
unk_8000A040::~unk_8000A040

Ends by setting the singleton to this and clearing it again, as retail does.
====================
*/
// 0x8000A390
unk_8000A040::~unk_8000A040() {
    fn_801A9E00( lbl_8052A780 );
    fn_801A9E00( lbl_8052A798 );
    fn_801A9E00( MsgStartBigRumble );
    fn_801A9E00( MsgStartSmallRumble );
    fn_801A9E00( MsgStopBigRumble );
    fn_801A9E00( MsgStopSmallRumble );
    fn_801A9E00( MsgStopAllRumble );
    fn_801A9E00( MsgSuspendRumble );
    fn_801A9E00( MsgResumeRumble );
    fn_801A9940( MsgStartBigRumble );
    fn_801A9940( MsgStartSmallRumble );
    fn_801A9940( MsgStopBigRumble );
    fn_801A9940( MsgStopSmallRumble );
    fn_801A9940( MsgStopAllRumble );
    fn_801A9940( MsgSuspendRumble );
    fn_801A9940( MsgResumeRumble );
    fn_801A9E00( MsgGetPadButtonMask );
    fn_801A9940( MsgGetPadButtonMask );

    lbl_80529160 = this;
    if ( lbl_80529160 != NULL ) {
        lbl_80529160 = NULL;
    }
}

/*
====================
unk_8000A040::fn_8000A6B4

Empty: called on each stick axis before it is used, the value passed by reference.
====================
*/
// 0x8000A6B4
void unk_8000A040::fn_8000A6B4( float& value ) {
}

/*
====================
fn_8000A6B8

Maps |x| from [deadZone, saturation] onto [0, 1] along a power curve and restores the sign of x.
====================
*/
// 0x8000A6B8
float fn_8000A6B8( float x, float deadZone, float saturation, float exponent ) {
    // RULE-EXCEPTION: R30, x carries the magnitude and then the result: with a separate result
    // local the allocator keeps it out of f1 and the function no longer matches.
    int sign = x < 0.0f ? -1 : 1;
    x *= sign;
    if ( x < deadZone ) {
        x = 0.0f;
    } else if ( x > saturation ) {
        x = 1.0f;
    } else {
        x = pow( ( x - deadZone ) / ( saturation - deadZone ), exponent );
    }
    x *= sign;
    return x;
}

/*
====================
unk_8000A040::fn_8000A76C

Passes the pad state of channel 0 to the debug value groups: the nunchuk stick, the buttons and
the pointer, averaged over the samples of this frame. Counts the time the pad has been left
alone and the time the pointer has been lost, and opens the HOME menu on a HOME press.
Not exact: in the pointer position block the display-height conversion takes the other stack
slot (0x18 for retail's 0x10) and the 640/320.0/0.5 constants and products take other FPRs;
statement orders of the block and the const display-height accessor do not close it. The pool
order is retail's only with the x line first (320.0 before 0.5), which this order lacks; written
x-first with the two scale products as double locals the volatile FPRs match and scale/x swap
f28/f29 (99.77%); an inline taking both values is not inlined.
====================
*/
// 0x8000A76C
void unk_8000A040::fn_8000A76C( float dt ) {
    static u32 lastHome;    // the HOME button state of the previous frame (name invented)
    u32 home;
    unk_PadChannel& pad = lbl_804579C0[0];
    u32 buttons = pad.samples[0].hold;
    bool idle = true;
    float stick = 0.0f;

    fn_800067A0();
    fn_8000AE80();  // retail discards the result

    if ( pad.samples[0].dev_type == WPAD_DEV_FS ) {
        stick = -pad.samples[0].ex_status.fs.stick.x;
        fn_8000A6B4( stick );
        fn_8000BF24( 0, stick );
        fn_8000BF24( 4, stick >= 0.3f );
        fn_8000BF24( 5, stick <= -0.3f );
        if ( fn_8000AE88( stick ) > 0.05f ) {
            idle = false;
        }

        stick = pad.samples[0].ex_status.fs.stick.y;
        fn_8000A6B4( stick );
        fn_8000BF24( 1, stick );
        fn_8000BF24( 2, stick >= 0.3f );
        fn_8000BF24( 3, stick <= -0.3f );
        if ( fn_8000AE88( stick ) > 0.05f ) {
            idle = false;
        }
    }

    fn_8000BF24( 6, ( buttons & WPAD_BUTTON_UP ) ? 1.0f : -1.0f );
    fn_8000BF24( 7, ( buttons & WPAD_BUTTON_DOWN ) ? 1.0f : -1.0f );
    fn_8000BF24( 8, ( buttons & WPAD_BUTTON_LEFT ) ? 1.0f : -1.0f );
    fn_8000BF24( 9, ( buttons & WPAD_BUTTON_RIGHT ) ? 1.0f : -1.0f );
    fn_8000BF24( 10, ( buttons & WPAD_BUTTON_A ) ? 1.0f : -1.0f );
    fn_8000BF24( 11, ( buttons & WPAD_BUTTON_B ) ? 1.0f : -1.0f );
    fn_8000BF24( 12, ( buttons & WPAD_BUTTON_C ) ? 1.0f : -1.0f );
    fn_8000BF24( 13, ( buttons & WPAD_BUTTON_Z ) ? 1.0f : -1.0f );
    fn_8000BF24( 14, ( buttons & WPAD_BUTTON_PLUS ) ? 1.0f : -1.0f );
    fn_8000BF24( 15, ( buttons & WPAD_BUTTON_MINUS ) ? 1.0f : -1.0f );
    home = buttons & WPAD_BUTTON_HOME;
    fn_8000BF24( 16, home ? 1.0f : -1.0f );
    fn_8000BF24( 17, ( buttons & WPAD_BUTTON_1 ) ? 1.0f : -1.0f );
    fn_8000BF24( 18, ( buttons & WPAD_BUTTON_2 ) ? 1.0f : -1.0f );
    if ( buttons & ( WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT | WPAD_BUTTON_DOWN | WPAD_BUTTON_UP | WPAD_BUTTON_PLUS
                     | WPAD_BUTTON_2 | WPAD_BUTTON_1 | WPAD_BUTTON_B | WPAD_BUTTON_A | WPAD_BUTTON_MINUS
                     | WPAD_BUTTON_Z | WPAD_BUTTON_C | WPAD_BUTTON_HOME ) ) {
        idle = false;
    }

    // a channel that has just started being read counts as use
    if ( !field_0x5138 && pad.field_0x0 ) {
        idle = false;
    }
    field_0x5138 = pad.field_0x0;
    if ( idle ) {
        field_0x5134 += dt;
    } else {
        field_0x5134 = 0.0f;
    }

    // average the pointer over up to 8 samples; with field_0x5139 set, a sample without the
    // pointer counts as the centre of the screen
    float sumX = 0.0f;
    float sumY = 0.0f;
    int count = 0;
    for ( int i = 0; i < 16 && count < 8; i++ ) {
        if ( pad.samples[i].dpd_valid_fg == 2 ) {
            sumX += pad.samples[i].pos.x;
            sumY += pad.samples[i].pos.y;
            count++;
        } else if ( field_0x5139 ) {
            sumX += 320.0f;
            sumY += 0.5f * lbl_8052861C;
            count++;
        }
    }

    if ( count > 4 ) {
        sumX /= count;
        sumY /= count;
        float scale = 1.0f / lbl_8045A680.range;
        float width = 640.0f * fn_8000AE94( fn_8000AE80() );
        float y = sumY * ( 0.5 * width * scale );
        float x = sumX * ( 320.0 * scale );
        x += 320.0f;
        y += 0.5f * lbl_8052861C;
        fn_8000BF24( 19, x );
        fn_8000BF24( 20, y );
        fn_8000BF24( 21, 1.0f );
        fn_8000BF24( 24, -1.0f );
        fn_8000BF24( 26, -( sumX * scale ) );
        fn_8000BF24( 25, -( sumY * scale ) );
        sumX *= scale;
        sumY *= 2.0f * scale;
        float aimX = fn_8000A6B8( sumX, lbl_8045A680.xDeadZone, lbl_8045A680.xSaturation, lbl_8045A680.xExponent );
        float aimY = fn_8000A6B8( sumY, lbl_8045A680.yDeadZone, lbl_8045A680.ySaturation, lbl_8045A680.yExponent );
        fn_8000BF24( 22, -aimX );
        fn_8000BF24( 23, -aimY );
        field_0x5124 = x;
        field_0x511C = -aimX;
        field_0x5120 = -aimY;
        field_0x5128 = y;
        field_0x512C = 0.0f;
    } else if ( field_0x512C < field_0x5130 ) {
        // the pointer was lost a moment ago: hold its last values
        field_0x512C += dt;
        fn_8000BF24( 21, 1.0f );
        fn_8000BF24( 24, 1.0f );
        if ( pad.samples[0].dev_type != WPAD_DEV_CLASSIC ) {
            fn_8000BF24( 19, field_0x5124 );
            fn_8000BF24( 20, field_0x5128 );
            fn_8000BF24( 22, field_0x511C );
            fn_8000BF24( 23, field_0x5120 );
        }
    } else {
        fn_8000BF24( 21, -1.0f );
        fn_8000BF24( 24, 1.0f );
        if ( pad.samples[0].dev_type != WPAD_DEV_CLASSIC ) {
            fn_8000BF24( 22, 0.0f );
            fn_8000BF24( 23, 0.0f );
        }
    }

    fn_8000AF08( 0 );
    if ( home && !( lastHome & WPAD_BUTTON_HOME ) ) {
        fn_8018E154();
    }
    lastHome = home;
}

/*
====================
fn_8000AE80
====================
*/
// 0x8000AE80
unk_800E671C* fn_8000AE80() {
    return lbl_80529E44;
}

/*
====================
fn_8000AE88
====================
*/
// 0x8000AE88
float fn_8000AE88( float x ) {
    return fabs( x );
}

/*
====================
fn_8000AE94

Horizontal scale of the display: 1 when field_0x251 is set, else from the aspect ratio
and the display height.
====================
*/
// 0x8000AE94
float fn_8000AE94( unk_800E671C* display ) {
    if ( display->field_0x251 ) {
        return 1.0f;
    }
    float aspect = display->field_0x250 ? 16.0f / 9.0f : 4.0f / 3.0f;
    float scale = 1.0f / ( aspect / ( 640.0f / lbl_8052861C ) );
    return scale * lbl_80528628;
}

/*
====================
unk_8000A040::fn_8000AF08

Feeds the remote and nunchuk motion of channel 0 to the two queues, updates field_0xA4 and
passes its fourteen values to debug value groups 27 to 40.
Not exact: in each inlined fn_8000BF24 retail keeps the table base in the row register and folds
the group offset into the loads (lwz 0x510(r29), addi 4); this source materialises the row (lwzu).
Ruled out: a loop over the fourteen groups (not unrolled); in fn_8000BF24 an index into the row
or the table (lbl_803DF270[group][++i], row[++i]: the index is not reduced to a pointer, and the
out-of-line copy falls), &lbl_803DF270[group][0], lbl_803DF270[0] + group * 12, a const row, and
the forms `row++, index = *row` (92.2%, still lwzu) and `( index = *row ) != TWEAK_END`.
====================
*/
// 0x8000AF08
void unk_8000A040::fn_8000AF08( int a ) {
    fn_8000B5DC( 0, 0, &field_0xC.field_0x0 );
    fn_8000B5DC( 0, 1, &field_0xC.field_0x34 );
    field_0xA4.fn_80007384( a );
    fn_8000BF24( 27, field_0xA4.fn_80008580( 0 ) );
    fn_8000BF24( 28, field_0xA4.fn_80008580( 1 ) );
    fn_8000BF24( 29, field_0xA4.fn_80008580( 2 ) );
    fn_8000BF24( 30, field_0xA4.fn_80008580( 3 ) );
    fn_8000BF24( 31, field_0xA4.fn_80008580( 4 ) );
    fn_8000BF24( 32, field_0xA4.fn_80008580( 5 ) );
    fn_8000BF24( 33, field_0xA4.fn_80008580( 6 ) );
    fn_8000BF24( 34, field_0xA4.fn_80008580( 7 ) );
    fn_8000BF24( 35, field_0xA4.fn_80008580( 8 ) );
    fn_8000BF24( 36, field_0xA4.fn_80008580( 9 ) );
    fn_8000BF24( 37, field_0xA4.fn_80008580( 10 ) );
    fn_8000BF24( 38, field_0xA4.fn_80008580( 11 ) );
    fn_8000BF24( 39, field_0xA4.fn_80008580( 12 ) );
    fn_8000BF24( 40, field_0xA4.fn_80008580( 13 ) );
}

/*
====================
unk_8000A040::fn_8000B5DC

Adds the motion samples of this frame to queue, oldest first, keeping its last 64 samples;
while every new sample is still, takes their mean acceleration as gravity (field_0x1C) and the
mean of the steadier set as field_0x28, then subtracts gravity from the new samples.
Not exact: register allocation differs (two more saved registers: the zero constants of the
inlined deque code are kept in separate registers, and the sample index is not rematerialised).
====================
*/
// MATCH-PENDING (R18): V3dAdd and V3dDivide are paired-single inline asm (C cannot express
// ps_add/ps_div), the same open question as CAmbientInterferencePoint's vector helpers.
// Not a const-parameter effect: queue is written.
// 0x8000B5DC
void unk_8000A040::fn_8000B5DC( int channel, int source, unk_PadQueue* queue ) {
    int count = 0;
    for ( int i = 0; i < lbl_804579C0[channel].sampleCount; i++ ) {
        if ( HasMotion( lbl_804579C0[channel].samples[i], source ) ) {
            count++;
        }
    }
    if ( count == 0 ) {
        return;
    }

    queue->field_0x18 = count;
    int excess = count + queue->events.size() - lbl_8052B6F8;
    excess = unk_Clamp( excess, 0, lbl_8052B6F8 );
    for ( int i = 0; i < excess; i++ ) {
        queue->events.pop_back();
    }

    for ( int i = 0; i < lbl_804579C0[channel].sampleCount; i++ ) {
        int index = count - 1 - i;
        if ( HasMotion( lbl_804579C0[channel].samples[index], source ) ) {
            unk_PadEvent event;
            if ( source == 0 ) {
                const KPADStatus& status = lbl_804579C0[channel].samples[index];
                event.acc.x = -status.acc.x;
                event.acc.y = status.acc.y;
                event.acc.z = status.acc.z;
                event.value = status.acc_value;
                event.speed = status.acc_speed;
            } else {
                const KPADStatus& status = lbl_804579C0[channel].samples[index];
                event.acc.x = status.ex_status.fs.acc.x;
                event.acc.y = status.ex_status.fs.acc.y;
                event.acc.z = status.ex_status.fs.acc.z;
                event.value = status.ex_status.fs.acc_value;
                event.speed = status.ex_status.fs.acc_speed;
            }
            queue->events.push_front( event );
        }
    }

    bool still = true;
    bool steady = true;
    for ( int i = 0; i < count; i++ ) {
        if ( queue->events[i].speed > 0.1f ) {
            still = false;
        }
        if ( queue->events[i].speed > 0.3f ) {
            steady = false;
        }
    }

    if ( still ) {
        queue->field_0x1C = unk_Vec3( 0.0f, 0.0f, 0.0f );
        for ( int i = 0; i < count; i++ ) {
            // BUG: sums the first sample count times instead of each new sample
            unk_PadEvent event = queue->events[0];
            unk_Vec3 acc( event.acc.x, event.acc.y, event.acc.z );
            V3dAdd( &queue->field_0x1C, &queue->field_0x1C, &acc );
        }
        V3dDivide( &queue->field_0x1C, &queue->field_0x1C, count );
    }
    if ( steady ) {
        queue->field_0x28 = unk_Vec3( 0.0f, 0.0f, 0.0f );
        for ( int i = 0; i < count; i++ ) {
            // BUG: as above
            unk_PadEvent event = queue->events[0];
            unk_Vec3 acc( event.acc.x, event.acc.y, event.acc.z );
            V3dAdd( &queue->field_0x28, &queue->field_0x28, &acc );
        }
        V3dDivide( &queue->field_0x28, &queue->field_0x28, count );
    }

    for ( int i = 0; i < count; i++ ) {
        queue->events[i].acc.x -= queue->field_0x1C.x;
        queue->events[i].acc.y -= queue->field_0x1C.y;
        queue->events[i].acc.z -= queue->field_0x1C.z;
    }
}

/*
====================
fn_8000BE94
====================
*/
// 0x8000BE94
unk_PadQueues* fn_8000BE94() {
    return &lbl_80529160->field_0xC;
}

/*
====================
fn_8000BEA0
====================
*/
// 0x8000BEA0
void fn_8000BEA0() {
    unk_SetPosParam( 0.02f, 0.75f );
}

/*
====================
fn_8000BED8
====================
*/
// 0x8000BED8
void fn_8000BED8() {
    unk_SetPosParam( 0.05f, 0.3f );
}

/*
====================
fn_8000BF10
====================
*/
// 0x8000BF10
void fn_8000BF10( bool* a, bool* b ) {
    lbl_80529160->field_0xA4.fn_80007270( a, b );
}

/*
====================
unk_8000A040::fn_8000BF24

Sets every debug value of group to value: floats take it, switches are turned on when
it is at least 0.1.
====================
*/
// 0x8000BF24
void unk_8000A040::fn_8000BF24( int group, float value ) {
    int* row = lbl_803DF270[group];
    int index = *row;
    unk_TweakEntry* entries = fn_800067C0();

    for ( ; index != TWEAK_END; index = *++row ) {
        if ( entries[index].type == TWEAK_FLOAT ) {
            *( float* )entries[index].value = value;
        } else if ( entries[index].type == TWEAK_BOOL && value >= 0.1f ) {
            *( bool* )entries[index].value = true;
        }
    }
}

/*
====================
unk_8000A040::fn_8009CB1C

On the frame messages, updates the pad state, runs the rumble motor from the rumble level and
passes the buttons and the stick to lbl_805291E8; answers the button-mask query and applies the
rumble messages.
====================
*/
// 0x8000BFCC
void unk_8000A040::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == lbl_8052A780.record || msg.record == lbl_8052A798.record ) {
        const unk_80491B38* time = static_cast< unk_80491B38* >( msg.data );
        KPADStatus& status = lbl_804579C0[0].samples[0];

        field_0x8 = 0;
        fn_8000A76C( time->field_0x8 );
        if ( msg.record == lbl_8052A780.record ) {
            field_0x74.fn_800067CC( time->field_0x8 );
            bool rumble = field_0x5134 < 30.0f && field_0x74.GetValue() > 0.02f;
            if ( rumble && !field_0xA0 ) {
                WPADControlMotor( 0, WPAD_MOTOR_RUMBLE );
                field_0xA0 = true;
            } else if ( !rumble && field_0xA0 ) {
                WPADControlMotor( 0, WPAD_MOTOR_STOP );
                field_0xA0 = false;
            }
        } else {
            WPADControlMotor( 0, WPAD_MOTOR_STOP );
        }

        unk_PadInputSink* sink = lbl_805291E8;
        if ( sink != NULL && sink->field_0x28 ) {
            sink->fn_80016E94( 2, status.hold & WPAD_BUTTON_1 );
            sink->fn_80016E94( 3, -status.ex_status.fs.stick.x );
            sink->fn_80016E94( 4, status.ex_status.fs.stick.y );
            sink->fn_80016E94( 5, status.hold & WPAD_BUTTON_A );
            sink->fn_80016E94( 7, status.hold & WPAD_BUTTON_LEFT );
            sink->fn_80016E94( 8, status.hold & WPAD_BUTTON_RIGHT );
            sink->fn_80016E94( 9, status.hold & WPAD_BUTTON_UP );
            sink->fn_80016E94( 10, status.hold & WPAD_BUTTON_DOWN );
        }
    }

    if ( msg.record == MsgGetPadButtonMask.record ) {
        msg.data = reinterpret_cast< void* >( field_0x8 );
    }

    if ( msg.record == MsgStartBigRumble.record || msg.record == MsgStartSmallRumble.record ) {
        const RumbleStartData* data = static_cast< RumbleStartData* >( msg.data );
        field_0x74.fn_80006860( data->level, data->lifetime, data->rampTime );
    } else if ( msg.record == MsgStopBigRumble.record || msg.record == MsgStopSmallRumble.record ) {
        const RumbleStartData* data = static_cast< RumbleStartData* >( msg.data );
        field_0x74.fn_80006860( 0.0f, data->level, 0.0f );
    } else if ( msg.record == MsgStopAllRumble.record ) {
        field_0x74.fn_80006860( 0.0f, 0.0f, 0.0f );
    } else if ( msg.record == MsgSuspendRumble.record ) {
        field_0x74.fn_80006954();
        WPADControlMotor( 0, WPAD_MOTOR_STOP );
    } else if ( msg.record == MsgResumeRumble.record ) {
        field_0x74.fn_800069D8();
    }
}
