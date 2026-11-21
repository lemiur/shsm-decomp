#ifndef GAME_UNK_803E5ED0_H
#define GAME_UNK_803E5ED0_H

/*
 * A small interface at 0x34 of unk_803E5E78 (vtable lbl_803E5ED0, size 0x10): its vptr, then three
 * ints cleared by its constructor. Its vtable's first two words are zero in retail and are not
 * declared; the one slot it lists (fn_800719E4) sits at vtable offset 8. Name from the vtable
 * address (invented).
 */
class unk_803E5ED0 {
public:
                    unk_803E5ED0();
    virtual void    fn_800719E4();

    int             field_0x4;
    int             field_0x8;
    int             field_0xC;
};

// Constructor: the three ints are cleared after the vptr is stored.
inline unk_803E5ED0::unk_803E5ED0() {
    field_0x4 = 0;
    field_0x8 = 0;
    field_0xC = 0;
}

#endif
