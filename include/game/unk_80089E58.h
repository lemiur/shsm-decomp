#ifndef GAME_UNK_80089E58_H
#define GAME_UNK_80089E58_H

#include <revolution/types.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/plugin/matfx/multitexgcn.h>

extern "C" {
// Sets the GameCube multitexture callbacks on the atomic's materials whose effect is Effect_O_Ice_2.mte.
void fn_80089E58(RpAtomic* atomic);

// Defined in unit main/auto_03_80088E58_text (not yet reconstructed): the GameCube pre-render callback.
RpMaterial* fn_800891D8(RpMaterial* material, void* object, RxGameCubePipeData* pipeData, void* callBackData);
}

#endif
