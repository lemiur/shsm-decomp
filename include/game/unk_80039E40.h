#ifndef GAME_UNK_80039E40_H
#define GAME_UNK_80039E40_H

#include <revolution/types.h>

/*
 * The base of the one-vptr gameplay objects (vtable lbl_803E2548, size 0x20, constructor
 * 0x80039E40, destructor 0x80039EF8; five slots, destructor first). The object is 0x60 bytes with
 * its vptr at 0x5C, after the data the constructor sets; that data is padding here. Name from its
 * constructor address (invented); slot names are the addresses of the functions its vtable lists.
 */
class unk_80039E40 {
public:
    u8              field_0x0[0x5C];

                    unk_80039E40();
    virtual         ~unk_80039E40();
    virtual void    fn_8003AC14();
    virtual void    fn_8003AE68();
    virtual void    fn_80028730();
    virtual void    fn_8003AE98();
};

#endif
