#ifndef GAME_UNK_803E0A48_H
#define GAME_UNK_803E0A48_H

#include <revolution/types.h>

/*
 * A named string record, 0x10 bytes: the vptr, a copy of the name, the hash of the name and one more
 * word. Its vtable __vt__12unk_803E0A48 (size 0xC) lists only the destructor. The record is built
 * inline by fn_8001A100, which looks records up by the hash of their name. Name from its vtable
 * (invented); member names invented.
 */
class unk_803E0A48 {
public:
    virtual         ~unk_803E0A48();

    char*           name;       // 0x4: copy of the name, freed by the destructor
    u32             nameHash;   // 0x8: hash of the name, compared by fn_8001A100
    s32             field_0xC;  // 0xC
};

#endif
