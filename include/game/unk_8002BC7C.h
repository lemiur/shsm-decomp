#ifndef GAME_UNK_8002BC7C_H
#define GAME_UNK_8002BC7C_H

#include <revolution/types.h>

/*
 * The class whose constructor is 0x8002BC7C (class name invented: no class-name evidence has
 * been found). Its vtable lbl_803E1788 (size 0x1F8) is not declared yet, so the class holds only
 * the storage the reconstructed functions read: the bytes before the field at 0x394, which
 * include the vptr, are a gap until the vtable and the bases are recovered.
 */
class unk_8002BC7C {
public:
    u8                      field_0x0[0x394];
    s32                     field_0x394;
};

#endif
