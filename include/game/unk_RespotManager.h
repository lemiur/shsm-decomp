#ifndef GAME_UNK_RESPOTMANAGER_H
#define GAME_UNK_RESPOTMANAGER_H

#include <revolution/types.h>

/*
 * The keeper of the current respot zone, as RespotZoneTrigger uses it (the object behind
 * lbl_80529260; its unit, around 0x80030F34, is not yet reconstructed). Class name invented,
 * from RespotZoneTrigger's "RespotZone set as %s / %s / %s".
 */
class unk_RespotManager {
public:
    // defined in 0x80030F34: takes copies of the three names (the old copy at 0x54 is freed first)
    void            fn_80030F34( const char* a, const char* b, const char* c );
    // defined in 0x8002F808: FMVTrigger passes it the name of its film
    void            fn_8002F808( const char* film );
    // defined in 0x80030254: the player's field_0xC, 0 without a player
    u32             fn_80030254();
    // defined in 0x80030314: the name of the texture dictionary that goes with an asset, NULL if
    // none (CPreCache)
    const char*     fn_80030314( u32 level, const char* name );
};

extern unk_RespotManager* lbl_80529260;     // defined elsewhere, unit not yet reconstructed

#endif
