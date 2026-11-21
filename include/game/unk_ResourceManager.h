#ifndef GAME_UNK_RESOURCEMANAGER_H
#define GAME_UNK_RESOURCEMANAGER_H

#include <revolution/types.h>

/*
 * The resource manager behind lbl_8052A5C0 (unit at 0x801B0120 not yet reconstructed). Name
 * invented. Only the first word and the flag at 0x10 are modelled; the rest of the layout is not
 * reconstructed.
 */
struct unk_ResourceManager {
    u32     field_0x0;      // 0x0, read by fn_80015F8C
    u8      gap_0x4[0xC];   // not reconstructed
    u8      field_0x10;     // read by fn_80029444; meaning not yet known
};

extern unk_ResourceManager* lbl_8052A5C0;

#endif
