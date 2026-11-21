#ifndef GAME_UNK_80016200_H
#define GAME_UNK_80016200_H

#include <revolution/types.h>
#include <game/unk_803E0680.h>

/*
 * The class of 0x80016200's unit (constructor 0x80016200, destructor 0x80016250). Name invented from the
 * constructor's address: no string names it. It derives from unk_803E0680 at offset 0 and overrides that
 * vtable's slots; its own vtable is __vt__12unk_80016200 at 0x803E07F0. The constructor allocates a
 * 0x10000-byte block from the game heap into the member at 0x34. The object is 0x3C bytes, the size its
 * factory (0x80013408) allocates; the bytes past 0x38 are not recovered.
 */
class unk_80016200 : public unk_803E0680 {
public:
                    unk_80016200();                 // 0x80016200
    virtual         ~unk_80016200();                // 0x80016250
    virtual void    fn_80015230();                  // override, defined at 0x800161D8
    virtual void    fn_800163B0();                  // fills the pure slot, 0x800163B0
    virtual void    fn_800164F0();                  // fills the pure slot, 0x800164F0
    virtual void    fn_800162C0( s32 a, s32 b, s32 c, s32 d, s32 e, void (*handler)(), void* userData ); // fills the pure slot, 0x800162C0
    virtual void    fn_80016520( const char* name, int a, int b, int c ); // fills the pure slot, 0x80016520
    virtual void    fn_80016C58();                  // fills the pure slot, 0x80016C58
    virtual void    fn_800165B0();                  // fills the pure slot, 0x800165B0
    virtual void    fn_80014310();                  // override, defined at 0x80016644

    void*           field_0x34;
    u8              field_0x38[0x4];
};

#endif
