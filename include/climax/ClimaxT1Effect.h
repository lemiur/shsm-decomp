// Names and layout: Gormiti link map and DWARF (ClimaxT1Effect.obj)
#ifndef CLIMAX_CLIMAXT1EFFECT_H
#define CLIMAX_CLIMAXT1EFFECT_H

#include <rwsdk/plcore/balist.h>
#include <rwsdk/plcore/batypes.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/baworld.h>

/*
====================
wbCBData

What the callback that collects the world space bounds of a clump hands from atomic to atomic: whether
no atomic has been seen yet, and the corners of the box so far.
====================
*/
class wbCBData {
public:
    bool first;   // 0x00
    RwV3d* min;   // 0x04
    RwV3d* max;   // 0x08
};

void BoundsIntegrate( wbCBData* Extents, RwV3d& pos );
RpAtomic* ClimaxT1AtomicGetWorldSpaceBounds( RpAtomic* atomic, wbCBData* Extents );
RpAtomic* wbCB( RpAtomic* atomic, void* data );

extern "C" {
int ClimaxT1EffectPluginAttach();
RwSList* ClimaxT1EffectBuildUVAnimMaterialsList4W( RpWorld* world );
void ClimaxT1EffectAddUVAnimTime( RwSList* materialList, float time );
void ClimaxT1EffectUVAnimUpdate( RwSList* materialList );
RpClump* ClimaxT1ClumpGetWorldSpaceBounds( RpClump* clump, RwV3d* min, RwV3d* max );
void ClimaxT1ClumpStartRendering( RpClump* clump );
void ClimaxT1ClumpStopRendering( RpClump* clump );
}

#endif
