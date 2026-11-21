#ifndef GAME_UNK_80085094_H
#define GAME_UNK_80085094_H

#include <revolution/types.h>
#include <game/CHitEffectManager.h>

extern "C" {
// The bank's effect lookup, defined in 0x8008A934 (unit not yet reconstructed). The bank is passed as the
// first argument. Parameter names invented; the two flags are not read yet.
unk_Effect* fn_8008A934( unk_EffectBank* bank, const char* name, const char* kind, int flagFirst, int flagSecond );

// Looks up the effect named by the strings and stores its handle in *out. Returns true when the handle is
// nonzero. Parameter names invented.
bool fn_80085094( u32* out, const char* name, const char* kind );
}

#endif
