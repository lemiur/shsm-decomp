#ifndef GAME_EFFECTBANK_H
#define GAME_EFFECTBANK_H

#include <revolution/types.h>

/* A loaded effect looked up by name. Name invented; members unknown. */
struct unk_Effect {
    char        field_0x0[0x40];
    u32         field_0x40;
    char        field_0x44[0x61C];
    int         field_0x660;
    const char* field_0x664;        // effect kind, one of the HIT_ kind names
};

/* The owner of the loaded effects. Name invented; members unknown. */
class unk_EffectBank {
public:
    unk_Effect* fn_8008AF18( const char* name );   // defined in game/Effects.cpp (0x8008AF18)
};

extern unk_EffectBank* lbl_805296E0;

#endif
