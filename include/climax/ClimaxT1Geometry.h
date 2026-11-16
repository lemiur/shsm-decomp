// Names and layout: Gormiti link map and DWARF (ClimaxT1Geometry.obj)
#ifndef CLIMAX_CLIMAXT1GEOMETRY_H
#define CLIMAX_CLIMAXT1GEOMETRY_H

#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamateri.h>

/*
====================
ClimaxT1Geometry

The ClimaxT1 plugin data of a geometry.
====================
*/
class ClimaxT1Geometry {
public:
    int alphaVertex; // 0x0: nonzero when the geometry has vertex colours and animated materials
};

extern "C" {
int ClimaxT1GeometryHasAnimatedMaterials( RpGeometry* geometry );
void ClimaxT1GeometryUpdate();
int ClimaxT1GeometryPluginAttach();

// Defined by ClimaxT1Material.cpp: the animation index of a material, negative for none.
int ClimaxT1MaterialGetAnimation( RpMaterial* material );
}

#endif
