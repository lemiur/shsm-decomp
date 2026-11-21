#ifndef GAME_UNK_800A8100_H
#define GAME_UNK_800A8100_H

// unit name unconfirmed: class name invented from its constructor's address (0x800A8100, in a unit
// not yet reconstructed). No string names the class.

#include <revolution/types.h>
#include <climax/AttributeHandler.h>
#include <climax/Clump.h>
#include <climax/Utils.h>
#include <game/CGenericSpawner.h>
#include <game/unk_80074138.h>
#include <rwsdk/tool/anim/rtanim.h>

/*
 * An asset of an asset list, found by its type name (fn_801AED0C). The data at 0x14 is the asset's
 * clump or animation, by type. Name invented.
 */
struct unk_AssetEntry {
    u8              field_0x00[ 0x14 ];
    union {
        unk_RpClump*        clump;      // 0x14, of "rwID_CLUMP"
        RtAnimAnimation*    animation;  // 0x14, of "rwID_HANIMANIMATION"
    };
};

/* The animation hierarchy of a clump (fn_801B6AD8). Name and layout partly invented. */
struct unk_RpHAnimHierarchy {
    u32             flags;          // 0x0, RenderWare hierarchy flags
    u8              field_0x04[ 0x1C ];
    RtAnimInterpolator* interpolator; // 0x20, plays the clump's animation
};

// C linkage, unit not yet reconstructed (by their use here)
extern "C" {
// defined in 0x801AED0C: the asset of the named type in an asset list, or NULL
unk_AssetEntry* fn_801AED0C( RWS::AssetTracker::CAssetList* list, const char* typeName );
// defined in 0x8009EB2C: attaches the clump to the object
void fn_8009EB2C( void* attachment, unk_RpClump* clump );
}

// RWS::FileSystem::Process, unit not yet reconstructed: reads the asset list of an open stream (by its use here)
namespace RWS {
namespace FileSystem {
AssetTracker::CAssetList* Process( RwStream* stream, bool flag );
}
}

// RenderWare (C linkage), unit not yet reconstructed (signatures by their use here)
extern "C" {
unk_RpWorld*     RpWorldAddClump( unk_RpWorld* world, unk_RpClump* clump );
unk_RpHAnimHierarchy*   RpHAnimHierarchyAttach( unk_RpHAnimHierarchy* hierarchy );
void            RpHAnimHierarchyUpdateMatrices( unk_RpHAnimHierarchy* hierarchy );
}

/*
 * A clump loaded from a named resource stream into the world. The constructor (fn_800A8100) stores
 * the vtable __vt__12unk_800A8100 (0x803E9B40) and the clump is cloned from the one passed in; the member names are invented.
 */
class unk_800A8100 : public unk_SpawnerResource {
public:
    unk_800A8100( const char* name, unk_RpClump* clump, unk_RpWorld* world, void* attachment );

    /* Virtual hook called with the loaded clump; empty by default (fn_800A8640). */
    virtual void    fn_800A8640( unk_RpClump* clump );

    /* Loads the clump and its animation (fn_800A82E4); returns the clump. */
    unk_RpClump*    fn_800A82E4();

    unk_RpClump*                    clump;          // 0x4, the loaded clump
    unk_RpWorld*                    world;          // 0x8, the world the clump is added to
    void*                           cacheHandle;    // 0xC, the request released when loading ends
    void*                           stream;         // 0x10, the resource stream of the named asset
    void*                           attachment;     // 0x14, attached to the clump when it is loaded
    RWS::AssetTracker::CAssetList*  assets;         // 0x18, the asset list the stream holds
    char*                           name;           // 0x1C, a copy of the resource name (fn_801B67FC)
};

#endif
