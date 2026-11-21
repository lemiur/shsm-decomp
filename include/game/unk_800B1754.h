#ifndef GAME_UNK_800B1754_H
#define GAME_UNK_800B1754_H

#include <revolution/types.h>

/*
 * Placeholder class; the name is invented after its constructor fn_800B1754. Only the slots and
 * fields that game/unk_800B28E0 uses are declared, in retail vtable order (lbl_803EA524 slots
 * 0x34, 0x3C, 0x50). Open: the vtable has 25 slots, a second vptr at 0x4 with its own group
 * (slots 0x68 and 0x6C are overrides of that base and are not declared), and the rest of the
 * layout. Member names invented.
 */
class unk_800B1754 {
public:
    virtual u8 fn_800B28E8();
    virtual void* fn_800B28E0();
    virtual bool fn_800B28F0();

private:
    u8 field_0x4[0x4];
    void* field_0x8;
    u8 field_0xC[0x10];
    u8 field_0x1C;
};

#endif
