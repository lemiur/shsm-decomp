// Names and layout: Gormiti link map and DWARF (RwInterface.obj)
#ifndef CLIMAX_RWINTERFACE_H
#define CLIMAX_RWINTERFACE_H

#include <rwsdk/world/baclump.h>

class ClimaxP1ChainItem;

/*
====================
ClimaxP1AtomicData

The particle plugin's data of an atomic (its plugin ID is 0xA07, its size 0xC): the index of the
atomic's object in its effect entry, and the chain item built for it. Names invented.
====================
*/
struct ClimaxP1AtomicData {
    int                 index;      // 0x0: negative when the atomic is not a particle's
    ClimaxP1ChainItem*  item;       // 0x4
    int                 unk_8;      // 0x8
};

// defined in RwInterface.cpp (ParticleX), not yet reconstructed: the offset of the plugin's data in an
// atomic (-1 until the plugin is registered).
extern int lbl_80528858;

// defined in RwInterface.cpp (ParticleX), not yet reconstructed: the offset of the dictionary plugin's
// global data in the engine instance (-1 until registered). Gormiti's link map puts it, as
// RWPlugin_GlobalOffset, in RwInterface.obj's .sdata after the atomic offset.
extern int GlobalOffset;

// defined in RwInterface.cpp (ParticleX), not yet reconstructed: the particle index of an atomic,
// negative when the atomic is not a particle's.
int ClimaxP1AtomicGetIndex( RpAtomic* atomic );

#endif
