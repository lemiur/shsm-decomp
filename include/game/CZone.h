#ifndef GAME_CZONE_H
#define GAME_CZONE_H

// Unit name and boundary: class CZone (registry string, include/climax/AttributeHandler.h);
// constructor 0x8002598C. Only the members the ice material setup (0x80089D0C) uses are
// named; the rest of the zone is in its own unit, not yet reconstructed, and its bases are
// not declared here.

#include <revolution/types.h>
#include <rwsdk/world/baworld.h>
#include <rwsdk/plugin/matfx/multitexgcn.h>

class CZone {
public:
    u8          field_0x0[0xA4];    // not yet reconstructed
    RpWorld*    mWorld;             // 0xA4: the world the zone's sectors and materials come from
    u8          field_0xA8[0xAC];   // not yet reconstructed
    u32         mFlags;             // 0x154: bit 0x1 is set when the zone has ice materials
};

/*
 * Pre-render callbacks of the ice multitextures (Effect_O_Ice_1.mte and Effect_O_Ice_2.mte), in
 * the unit of their own addresses (not yet reconstructed).
 */
extern "C" {
RpMaterial* fn_8008914C( RpMaterial* material, void* object, RxGameCubePipeData* pipeData, void* callBackData );
RpMaterial* fn_800891D8( RpMaterial* material, void* object, RxGameCubePipeData* pipeData, void* callBackData );
}

extern "C" void fn_80089D0C( CZone* zone );

#endif
