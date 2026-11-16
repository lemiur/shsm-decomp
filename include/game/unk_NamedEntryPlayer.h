#ifndef GAME_UNK_NAMEDENTRYPLAYER_H
#define GAME_UNK_NAMEDENTRYPLAYER_H

#include <revolution/types.h>

/*
 * The singleton lbl_8052A0D0 points to (constructor 0x8014FBE8, unit not yet reconstructed).
 * fn_8014EC98 looks a name up by its string hash in a list of entries and processes the match;
 * the optional third argument receives two flag bytes. Name invented.
 */
/* The two flag bytes fn_8014EC98 writes to its optional result (roles unknown). Name invented. */
struct unk_NamedEntryResult {
    bool    field_0x0;
    bool    field_0x1;
};

class unk_NamedEntryPlayer {
public:
    // defined in 0x8014EC98, unit not yet reconstructed
    void    fn_8014EC98( const char* name, bool flag, void* result );
};

extern unk_NamedEntryPlayer* lbl_8052A0D0;  // defined elsewhere, unit not yet reconstructed

#endif
