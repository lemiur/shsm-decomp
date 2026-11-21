#ifndef GAME_UNK_803E0680_H
#define GAME_UNK_803E0680_H

#include <revolution/types.h>

/*
 * The primary base of unk_80016200 (at offset 0). Name invented from its vtable __vt__12unk_803E0680
 * at 0x803E0680 (vtable address; no string names the class). Its constructor (0x80012A68) installs
 * that vtable and the members below; its destructor is 0x80012C50. Its vtable has six pure slots,
 * named here after the functions that fill them in unk_80016200. Members not yet recovered keep their
 * offsets as placeholders. The class is 0x34 bytes long, as unk_80016200 places its own members at 0x34.
 */
class unk_803E0680 {
public:
                    unk_803E0680();                 // 0x80012A68
    virtual         ~unk_803E0680();                // 0x80012C50
    virtual void    fn_80015230();
    virtual void    fn_800163B0() = 0;
    virtual void    fn_800164F0() = 0;
    virtual void    fn_800162C0( s32 a, s32 b, s32 c, s32 d, s32 e, void (*handler)(), void* userData ) = 0;
    virtual void    fn_80016520( const char* name, int a, int b, int c ) = 0;
    virtual void    fn_80016C58() = 0;
    virtual void    fn_800165B0() = 0;
    virtual void    fn_80014990();
    virtual void    fn_800149B4();
    virtual void    fn_80013C34();
    virtual void    fn_80014310();

    s32             field_0x04;
    s32             field_0x08;
    void*           field_0x0C;
    s32             field_0x10;
    void*           field_0x14;
    s32             field_0x18;
    void*           field_0x1C;
    u8              field_0x20[0x10];
    s32             field_0x30;
};

#endif
