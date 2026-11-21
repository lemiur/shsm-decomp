#ifndef GAME_UNK_800A40B4_H
#define GAME_UNK_800A40B4_H

#include <game/unk_8009D2E4.h>
#include <climax/Atomic.h>

/*
 * The object built by its own constructor (class name invented: unk_<constructor address>).
 * It derives from unk_8009D2E4 (0x144 bytes). Only the members the atomic callback fn_800A4098
 * writes are declared here; the rest of the object stays padding until its constructor is
 * reconstructed.
 */
class unk_800A40B4 : public unk_8009D2E4 {
public:
    u8              field_0x144[0x98];
    unk_RpAtomic*   field_0x1dc;    // the first atomic found with a geometry (name invented)
    void*           field_0x1e0;    // that atomic's geometry (name invented)
};

#endif
