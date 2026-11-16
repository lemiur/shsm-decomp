#ifndef GAME_UNK_800E671C_H
#define GAME_UNK_800E671C_H

#include <revolution/types.h>

class unk_80114598;

/*
 * The game state object lbl_80529E44 points to (constructor 0x800E671C, unit not yet
 * reconstructed): display settings and the cameras. Name invented; only the members recovered
 * code reads are known.
 */
class unk_800E671C {
public:
    /*
     * The camera the view is on: the override at field_0x2A0 when it holds one, else the table's
     * entry for field_0x8. Inline at about twenty sites in ten units. Name invented.
     */
    unk_80114598*   GetCamera() const {
                        unk_80114598* camera = field_0x2A0 != NULL ? *field_0x2A0 : NULL;
                        return camera != NULL ? camera : field_0xC0[field_0x8];
                    }

    u8              field_0x0[0x8];
    int             field_0x8;          // the current camera: index into field_0xC0
    int             field_0xC;          // the number of cameras in field_0xC0
    u8              field_0x10[0xB0];
    unk_80114598*   field_0xC0[100];    // the cameras, registered by their constructor (fn_801152AC);
                                        // length unknown, at most up to 0x250
    bool            field_0x250;        // 16:9 display
    bool            field_0x251;
    u8              field_0x252[0x4E];
    unk_80114598**  field_0x2A0;        // when set and non-null, overrides the table's camera
};

extern unk_800E671C* lbl_80529E44;  // defined in 0x800E671C's unit, unit not yet reconstructed

#endif
