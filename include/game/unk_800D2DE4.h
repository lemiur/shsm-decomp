#ifndef GAME_UNK_800D2DE4_H
#define GAME_UNK_800D2DE4_H

#include <game/unk_80039E40.h>

/*
 * A gameplay object (vtable lbl_803EE1F4, size 0x1C, constructor 0x800D2DE4, destructor
 * 0x800D3254): unk_80039E40 and data of its own. Its constructor's one caller in retail is
 * 0x800E671C. Its destructor overrides a slot of
 * unk_80039E40. The object is 0x3B4 bytes; its own data is padding here.
 */
class unk_800D2DE4 : public unk_80039E40 {
public:
    virtual         ~unk_800D2DE4();  // override

    u8              field_0x60[0x354];
};

#endif
