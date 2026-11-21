#ifndef GAME_UNK_80073FB8_H
#define GAME_UNK_80073FB8_H

#include <game/AtomicTags.h>
#include <rwsdk/plcore/batkreg.h>
#include <rwsdk/world/baclump.h>

/*
 * One clump of a clump clone and the atomic tag types to take from it. Name invented.
 */
struct unk_ClumpTypeSpec {
    RpClump*                    clump;      /* 0x0 */
    AtomicTags::E_AtomicType*   types;      /* 0x4 */
    int                         numTypes;   /* 0x8 */
};

/* The clump plugin registry the clone copies its plugin data through (its size is sizeof(RpClump)). */
extern RwPluginRegistry lbl_803E6590;

extern "C" {
// defined in 0x80073FB8: clones the first spec's clump and adds to the copy the atomics of each spec's types
RpClump* fn_80073FB8( unk_ClumpTypeSpec* specs, int numSpecs );
}

#endif
