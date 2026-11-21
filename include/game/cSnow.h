#ifndef GAME_CSNOW_H
#define GAME_CSNOW_H

#include <game/CBaseBehaviour.h>
#include <climax/unk_801A92A8.h>

struct unk_80491B38;

/* One flake of the particle array: 16 bytes. Member names invented. */
struct unk_SnowFlake {
    f32             x;              // 0x0, position, spread about the emitter
    f32             y;              // 0x4
    f32             z;              // 0x8
    u8              field_0xC;      // 0xC, cleared when the flake is spawned
    u8              cell;           // 0xD, column | row << 4 of the 16 x 16 grid the flake starts in
    s16             field_0xE;      // 0xE, cleared when the flake is spawned
};

/*
 * The snow behaviour (vtable lbl_803E85B8, size 0x14C), registered as "cSnow" in fn_8008E4D0.
 * Name from the registry string. Members are named by their offset until their use is known.
 */
class cSnow : public CBaseBehaviour {
public:
                    cSnow();
    virtual         ~cSnow();
    virtual void    fn_8009CB1C( unk_Msg& msg );                        // override, slot 0x48
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override, slot 0x4C
    virtual void    fn_801A7A88();                                      // override, slot 0x18 / 0x58
    void            fn_8008C82C();

    unk_SnowFlake*  field_0x34;     // the particle array (10000 flakes), made in the constructor
    u32             field_0x38;     // its count
    unk_801A92A8    field_0x3C;
    unk_801A92A8    field_0x44;
    unk_801A92A8    field_0x4C;
    unk_801A92A8    field_0x54;
    unk_801A92A8    field_0x5C;
    unk_801A92A8    field_0x64;
    unk_801A92A8    field_0x6C;
    u8              field_0x74;
    u8              field_0x75;
    u8              field_0x76;
    u8              field_0x77;
    u8              field_0x78;
    u8              field_0x79;
    u8              field_0x7A;
    f32             field_0x7C;
    f32             field_0x80;
    f32             field_0x84;
    f32             field_0x88;
    f32             field_0x8C;
    f32             field_0x90;
    f32             field_0x94;
    f32             field_0x98;
    u8              field_0x9C[0x4];
    f32             field_0xA0;
    f32             field_0xA4;
    f32             field_0xA8;
    f32             field_0xAC;
    f32             field_0xB0;
    f32             field_0xB4;
    u8              field_0xB8;
    u8              field_0xB9[0x13];
    f32             field_0xCC;
    f32             field_0xD0;
    f32             field_0xD4;
    u32             field_0xD8;
    u32             field_0xDC;
    u32             field_0xE0;
    u8              field_0xE4[0xC];
    u8              field_0xF0;
    u8              field_0xF1;
    u8              field_0xF2[0x2];
    unk_RwV3d       field_0xF4;     // a point the snow tracks, transformed by a message's matrix
    f32             field_0x100;
    f32             field_0x104;
    f32             field_0x108;
    unk_801A92A8    field_0x10C;
    cSnow*          field_0x114;    // the next snow in the static list (lbl_805296F8)
    f32             field_0x118;
    f32             field_0x11C;
    f32             field_0x120;
    f32             field_0x124;
    u32             field_0x128;
    const char*     field_0x12C;    // the name of the texture the snow draws with (name invented)
    RwTexture*      field_0x130;    // that texture, found by FindTexture
    RwRaster*       field_0x134;    // its raster
    u8              field_0x138;
    u8              field_0x139;
    u8              field_0x13A[0x2];
    u32             field_0x13C;
    u32             field_0x140;
    u32             field_0x144;
};

/* The head of the list of live snows, linked through field_0x114. Defined in cSnow.cpp. */
extern cSnow* lbl_805296F8;

extern "C" {
void fn_8008D08C( cSnow* snow, unk_80491B38& player );
void fn_8008E120();
void fn_8008E1E8();
void fn_8008E268();
void fn_8008E2A4();
void fn_8008E36C();
void fn_8008E3DC();
void fn_8008E570();
void fn_8008E5B8();
void fn_8008E62C();
void fn_8008E634();
void fn_8008E63C();
void fn_8008E644();
}

#endif
