#ifndef GAME_UNK_803E5E78_H
#define GAME_UNK_803E5E78_H

#include <game/CBaseBehaviour.h>
#include <game/unk_803E5ED0.h>
#include <climax/unk_801A92A8.h>

/*
 * A behaviour with an interface object at 0x34 (unk_803E5ED0, a member: its vtable lbl_803E5ED0 is
 * separate from this class's, not a secondary group of it) and a flag, an int and a unk_801A92A8
 * member after it (vtable lbl_803E5E78, size 0x54). Its constructor sets the flag and clears the
 * int. Name from the vtable address (invented); the field names are their offsets.
 */
class unk_803E5E78 : public CBaseBehaviour {
public:
                    unk_803E5E78();

    unk_803E5ED0    field_0x34;
    bool            field_0x44;
    int             field_0x48;
    unk_801A92A8    field_0x4c;
};

// Constructor body: the flag is set and the int cleared once the members are constructed.
inline unk_803E5E78::unk_803E5E78() {
    field_0x44 = true;
    field_0x48 = 0;
}

#endif
