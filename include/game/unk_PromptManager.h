#ifndef GAME_UNK_PROMPTMANAGER_H
#define GAME_UNK_PROMPTMANAGER_H

// unit name unconfirmed: header named after the class's role (unit 0x800D7C00, not yet
// reconstructed).

#include <revolution/types.h>

/*
 * The on-screen prompt manager lbl_80529C78 points to (unit 0x800D7C00, not yet reconstructed):
 * shows and stops prompt strings by localisation id. Name invented; only the members recovered
 * code calls are known.
 */
class unk_PromptManager {
public:
    // defined in 0x800D84C4: shows the prompt with the localisation id (restarting it when it is
    // already listed); a is stored with it, flag and time are passed to the new entry. Returns
    // whether the prompt was already listed.
    bool    fn_800D84C4( u32 id, int a, bool flag, float time );
    // defined in 0x800D85CC: stops the prompt with the localisation id (any prompt when id is 0)
    void    fn_800D85CC( u32 id );
    // defined in 0x800D8DBC: always false in this build (role unknown)
    bool    fn_800D8DBC();
    // defined in 0x800D838C, unit not yet reconstructed: shows the named prompt for time seconds
    void    fn_800D838C( float time, const char* name, bool flag, bool flag2 );
};

extern unk_PromptManager* lbl_80529C78; // defined elsewhere, unit not yet reconstructed

#endif
