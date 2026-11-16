#ifndef GAME_UNK_8000A040_H
#define GAME_UNK_8000A040_H

#include <climax/EventHandler.h>
#include <climax/Memory.h>
#include <game/Display.h>
#include <game/unk_Deque.h>
#include <game/unk_800E671C.h>
#include <game/Messages.h>
#include <game/PadRumble.h>
#include <game/Vec3.h>
#include <revolution/kpad.h>
#include <string.h>

// defined in 0x80006C7C-0x8000A040, unit not yet reconstructed.
class unk_80006C7C {
public:
                    unk_80006C7C();     // 0x80006C7C
                    ~unk_80006C7C();    // 0x80006D34
    void            fn_80007270( int a, int b );
    void            fn_80007384( int a );
    float           fn_80008580( int index );

    u32             field_0x0[0x5078 / 4];
};

/*
 * Defined in 0x80016C6C's unit, unit not yet reconstructed: takes the pad state as numbered
 * input values while enabled. Name invented; members unknown.
 */
class unk_PadInputSink {
public:
    void            fn_80016E94( int input, float value );

    u8              field_0x0[0x28];
    bool            field_0x28;     // enabled
};

extern unk_PadInputSink* lbl_805291E8;  // defined elsewhere, unit not yet reconstructed

/*
 * The KPAD state of one channel (lbl_804579C0): the samples of the latest KPADRead, newest
 * first, filled in 0x80006C7C's unit. Name invented.
 */
struct unk_PadChannel {
    bool            field_0x0;          // the channel is read
    KPADStatus      samples[16];        // 0x008
    u32             field_0xB08;
    s32             sampleCount;        // 0xB0C, the result of KPADRead
    bool            field_0xB10;
    u8              field_0xB11[0x1F];
};

extern unk_PadChannel lbl_804579C0[4];

/* Pointer tuning: the pointer range and the response curve of each axis (fn_8000A6B8). Name invented. */
struct unk_PointerTuning {
    float           range;
    float           xDeadZone;
    float           xSaturation;
    float           xExponent;
    float           yDeadZone;
    float           ySaturation;
    float           yExponent;
    float           field_0x1C;
};

extern unk_PointerTuning lbl_8045A680;

void            fn_8018E154();  // defined elsewhere, unit not yet reconstructed; called when HOME is pressed

/* Sets the pointer parameters of pad 0. Name invented. */
inline void unk_SetPosParam( float playRadius, float sensitivity ) {
    KPADSetPosPlayMode( 0, KPAD_PLAY_MODE_LOOSE );
    KPADSetPosParam( 0, playRadius, sensitivity );
}

extern float lbl_80528628;  // 1.0; defined elsewhere, unit not yet reconstructed

/*
 * A motion sample of the unk_8000A040 queues: a copy of KPADStatus's acc, acc_value and acc_speed
 * (the same three members, in the same order). Name invented; member names after KPADStatus.
 */
struct unk_PadEvent {
    Vec         acc;        // 0x00
    float       value;      // 0x0C
    float       speed;      // 0x10
};

/* One queue of unk_8000A040 and its state. Name invented. */
struct unk_PadQueue {
    unk_Deque< unk_PadEvent, 64 >   events;     // 0x00
    int         field_0x18;     // samples added by the last fn_8000B5DC
    unk_Vec3    field_0x1C;     // the mean acceleration while the pad is still: gravity
    unk_Vec3    field_0x28;

    void        Reset() {
        field_0x1C = unk_Vec3( 0.0f, -1.0f, 0.0f );
        field_0x18 = 0;
    }
};

/* The two queues at unk_8000A040 + 0x0C, returned by fn_8000BE94. Name invented. */
struct unk_PadQueues {
    unk_PadQueue    field_0x0;
    unk_PadQueue    field_0x34;
};

/*
 * A pad and pointer manager: a singleton (lbl_80529160) that receives the rumble
 * messages and sets KPAD parameters. No string names it: placeholder.
 */
class unk_8000A040 : public CEventHandler {
public:
                    unk_8000A040();
    virtual         ~unk_8000A040();
    virtual void    fn_8009CB1C( unk_Msg& msg );

    void            fn_8000A6B4( float& value );
    void            fn_8000A76C( float dt );
    void            fn_8000AF08( int a );
    void            fn_8000B5DC( int channel, int source, unk_PadQueue* queue );
    void            fn_8000BF24( int group, float value );

    int             field_0x8;
    unk_PadQueues   field_0xC;
    unk_Ramp        field_0x74;     // the rumble level
    bool            field_0xA0;
    unk_80006C7C    field_0xA4;
    float           field_0x511C;   // the last pointer values, held while the pointer is lost
    float           field_0x5120;
    float           field_0x5124;
    float           field_0x5128;
    float           field_0x512C;   // seconds since the pointer was lost
    float           field_0x5130;
    float           field_0x5134;   // seconds since the pad was last used
    bool            field_0x5138;
    bool            field_0x5139;
};

/* Value end marker of a lbl_803DF270 row: one past the last debug value. Name invented. */
#define TWEAK_END   86

/* Groups of debug values: indices into lbl_803DEF20, each row ended by TWEAK_END. */
extern int lbl_803DF270[41][12];

extern unk_8000A040* lbl_80529160;

void            fn_8000A028( bool value );
bool            fn_8000A02C();
void            fn_8000A034( bool value );
unk_800E671C*   fn_8000AE80();
float           fn_8000AE88( float x );
float           fn_8000AE94( unk_800E671C* display );
float           fn_8000A6B8( float x, float deadZone, float saturation, float exponent );
unk_PadQueues*  fn_8000BE94();
void            fn_8000BEA0();
void            fn_8000BED8();
void            fn_8000BF10( int a, int b );

#endif
