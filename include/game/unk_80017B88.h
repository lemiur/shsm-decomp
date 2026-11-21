#ifndef GAME_UNK_80017B88_H
#define GAME_UNK_80017B88_H

#include <revolution/types.h>
#include <climax/unk_801A92A8.h>

/*
 * Element of the 0x100-entry array that unk_8001777C::fn_80017DD8 builds with __construct_new_array
 * (constructor 0x80017E6C, element size 0x14, one vtable slot: the destructor 0x80017B88, vtable
 * lbl_803E0A54). Name invented. The constructor zeroes field_0x4 and field_0x8; field_0xC is not
 * touched by either special function.
 */
class unk_80017E6C {
public:
                    unk_80017E6C();
    virtual         ~unk_80017E6C();

    s32             field_0x4;
    u8*             field_0x8;
    u8              field_0xC[0x8];
};

/*
 * Animation::s_MountRef, the class whose destructor 0x80017C00 destroys the unk_801A92A8 member at
 * 0xB4. The rest of the object is padding here. Name invented apart from the mangled name.
 */
class Animation {
public:
    class s_MountRef {
    public:
                    ~s_MountRef();

        u8              field_0x0[0xB4];
        unk_801A92A8    field_0xB4;
    };
};

#endif
