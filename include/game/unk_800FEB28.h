#ifndef GAME_UNK_800FEB28_H
#define GAME_UNK_800FEB28_H

#include <game/unk_80039E40.h>

/*
 * A one-vptr gameplay object (vtable lbl_803F2314, size 0x1C, constructor 0x800FEB28, destructor
 * 0x800FEB70): unk_80039E40 and two words of its own. Its destructor and fn_8003AE98 (0x800FEE58)
 * override slots of unk_80039E40. The object is 0x68 bytes; its own data is padding here.
 */
class unk_800FEB28 : public unk_80039E40 {
public:
    virtual         ~unk_800FEB28();  // override
    virtual void    fn_8003AE98();  // override

    u8              field_0x60[0x8];
};

#endif
