#ifndef GAME_UNK_800AF100_H
#define GAME_UNK_800AF100_H

#include <revolution/types.h>
#include <game/unk_ResourceManager.h>

/*
 * The ring-buffer read stream the async file reader fills (name invented). Its members are not
 * reconstructed here; this unit only passes it on.
 */
struct unk_AsyncStream;

extern "C" {
u32 fn_800AF100( unk_AsyncStream* stream, u32* dst, u32 size );
}

#endif
