#ifndef GAME_UNK_80006C7C_H
#define GAME_UNK_80006C7C_H

#include <revolution/types.h>

/*
 * The class of 0x80006C7C's unit (0x80006C7C-0x80008828). Name invented: no string names it.
 * Members not yet recovered keep their offsets as placeholders. The constructor clears the
 * table at 0x78 with memset and sets the fields below; the class is 0x5078 bytes, as its use
 * in unk_8000A040 (field_0xA4) shows.
 */
class unk_80006C7C {
public:
                    unk_80006C7C();     // 0x80006C7C
                    ~unk_80006C7C();    // 0x80006D34
    void            fn_80007270( bool* a, bool* b );
    void            fn_80007384( int a );
    float           fn_80008580( int index );

    s32             field_0x00;
    s32             field_0x04;
    s32             field_0x08;
    float           field_0x0C;
    float           field_0x10;
    float           field_0x14;
    float           field_0x18;
    s32             field_0x1C;
    u8              field_0x20[0x4];
    s32             field_0x24;
    s32             field_0x28;
    s32             field_0x2C;
    s32             field_0x30;
    s32             field_0x34;
    float           field_0x38;
    float           field_0x3C;
    float           field_0x40;
    float           field_0x44;
    s32             field_0x48;
    u8              field_0x4C[0x4];
    s32             field_0x50;
    s32             field_0x54;
    float           field_0x58;
    float           field_0x5C;
    float           field_0x60;
    float           field_0x64;
    s32             field_0x68;
    s32             field_0x6C;
    s32             field_0x70;
    bool            field_0x74;
    bool            field_0x75;
    u8              field_0x76[0x2];
    u8              field_0x78[0x5000];
};

extern "C" {
void fn_80006D74();
}

#endif
