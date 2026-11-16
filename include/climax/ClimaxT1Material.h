#ifndef CLIMAX_CLIMAXT1MATERIAL_H
#define CLIMAX_CLIMAXT1MATERIAL_H

// Names and layout: Gormiti link map and DWARF (ClimaxT1Material.obj).

#include <rwsdk/world/bamateri.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/tool/anim/rtanim.h>

/*
====================
ClimaxT1Material

The Climax plugin data of a RenderWare material (RpMaterial): its sidedness, frame blending and
animation, and the animation's interpolator and UV transform matrices once they are needed. The
plugin's data sits LocalOffset bytes into the material.
====================
*/
class ClimaxT1Material {
public:
    int                 isDoubleSided;      // 0x00
    int                 frameBlendMode;     // 0x04, the low 16 bits are set from streams (the high 16 are kept)
    int                 animation;          // 0x08, index in the T1 dictionary's animations, or -1
    RtAnimInterpolator* interpolator;       // 0x0C, owned
    RwMatrix*           UvMatrix;           // 0x10, owned: two matrices (base, dual), or NULL
};

extern "C" {

int             ClimaxT1MaterialGetIsDoubleSided( RpMaterial* material );
int             ClimaxT1MaterialGetFrameBlendMode( RpMaterial* material );
int             ClimaxT1MaterialGetAnimation( RpMaterial* material );
RwMatrix*       ClimaxT1MaterialGetUVMatrices( RpMaterial* material );
int             ClimaxT1MaterialPluginAttach();

}

#endif
