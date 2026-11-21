#ifndef GAME_UNK_803E5E20_H
#define GAME_UNK_803E5E20_H

#include <game/CBaseBehaviour.h>
#include <climax/unk_801A92A8.h>

/*
 * A behaviour with a unk_801A92A8 member at 0x34 and two scalars after it (vtable lbl_803E5E20,
 * size 0x48). The constructor clears the int and the float. The 4 bytes at 0x44 are never touched
 * by recovered code; they make up the allocated size. Name from the vtable address (invented); the
 * field names are their offsets.
 */
class unk_803E5E20 : public CBaseBehaviour {
public:
                    unk_803E5E20();

    unk_801A92A8    field_0x34;
    int             field_0x3c;
    float           field_0x40;
    u8              field_0x44[0x4];
};

/*
 * Constructor body: the member is constructed first, then the int and the float are cleared.
 */
inline unk_803E5E20::unk_803E5E20() {
    field_0x3c = 0;
    field_0x40 = 0.0f;
}

#endif
