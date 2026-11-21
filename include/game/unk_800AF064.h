#ifndef GAME_UNK_800AF064_H
#define GAME_UNK_800AF064_H

#include <revolution/types.h>
#include <game/unk_ResourceManager.h>

// Names of this unit's functions are the retail symbols (no mangled form yet): the parameter
// types and names are inferred from the call sites, not from the original source. fn_801B10AC is
// declared in game/unk_ResourceManager.h.
extern "C" {
u32 fn_800AF064( void* reader, void* buffer, u32 count );
u32 fn_800AF080( void* reader, void* buffer, u32 count );
u32 fn_800AF088( void* reader, void* buffer, u32 count );
u32 fn_800AF090( void* reader, void* buffer, u32 count );
}

#endif
