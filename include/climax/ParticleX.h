// Names and layout: Gormiti link map and DWARF (ParticleX: PxEmitter.obj, PxObjects.obj, RwPlugin.obj).
#ifndef CLIMAX_PARTICLEX_H
#define CLIMAX_PARTICLEX_H

#include <rwsdk/world/baclump.h>

class ParticleX_Emitter_Frame;
class ParticleX_Container_Frame;
class ParticleX_Field_Frame;
class ParticleX_Emitter_Vertex_Item;
class ClimaxP1ChainTemplate;
class ClimaxP1ChainItem;

// The types of the objects an effect entry holds (ParticleXObject::Type); names invented.
enum {
    PARTICLEX_EMITTER_BASIC = 0,
    PARTICLEX_EMITTER_SPHERE = 1,
    PARTICLEX_EMITTER_TORUS = 2,
    PARTICLEX_EMITTER_VERTEX = 3,
    PARTICLEX_EMITTER_CONTAINER = 4,
    PARTICLEX_CONTAINER = 5,
    PARTICLEX_FIELD = 6,
    PARTICLEX_FIELD_LOCAL = 7
};

/*
====================
ParticleXObject

The common head of the objects of an effect entry. RawData and ChildRawData are the object's
attribute blocks (ClimaxS1Lib), freed once the object is set up.
====================
*/
class ParticleXObject {
public:
    int         Type;               // 0x00
    char*       RawData;            // 0x04
    char*       ChildRawData;       // 0x08
    RpAtomic*   Atomic;             // 0x0C
};

class ParticleX_Emitter : public ParticleXObject {
public:
    int                         FrameCount;         // 0x10
    ParticleX_Emitter_Frame*    Frames;             // 0x14
    ParticleX_Emitter_Frame**   FramesRef;          // 0x18
    unsigned int                BaseEmitFrames;     // 0x1C
    unsigned int                BaseEmitFramesF;    // 0x20
    float                       emitterTimeMax;     // 0x24
    ClimaxP1ChainTemplate*      ChainTemplate;      // 0x28
};

class ParticleX_Emitter_Basic : public ParticleX_Emitter {
public:
    void Setup();
};

class ParticleX_Emitter_Sphere : public ParticleX_Emitter {
public:
    void Setup();
};

class ParticleX_Emitter_Torus : public ParticleX_Emitter {
public:
    void Setup();
};

class ParticleX_Emitter_Vertex : public ParticleX_Emitter {
public:
    int                             FullEmit;       // 0x2C
    int                             NumVertex;      // 0x30
    ParticleX_Emitter_Vertex_Item*  VertexData;     // 0x34

    void Setup();
};

class ParticleX_Emitter_Container : public ParticleX_Emitter {
public:
    void Setup();
};

class ParticleX_Container : public ParticleXObject {
public:
    int                         maxParticles;                   // 0x10
    bool                        EmitterInheritContainerTransform; // 0x14
    int                         rows;                           // 0x18
    int                         columns;                        // 0x1C
    float                       delta_row;                      // 0x20
    float                       delta_column;                   // 0x24
    int                         span;                           // 0x28
    int                         FrameCount;                     // 0x2C
    ParticleX_Container_Frame*  Frames;                         // 0x30
    ParticleX_Container_Frame** FramesRef;                      // 0x34

    void Setup();
};

class ParticleX_Field : public ParticleXObject {
public:
    int                     FrameCount;     // 0x10
    ParticleX_Field_Frame*  Frames;         // 0x14

    void Setup();
};

/*
====================
ClimaxP1Entry

An effect entry: the particle objects of one effect, shared by reference count.
====================
*/
class ClimaxP1Entry {
public:
    int                 refCount;       // 0x00
    int                 NumObjects;     // 0x04
    ParticleXObject**   ObjectPtrs;     // 0x08
};

ClimaxP1Entry*          ClimaxP1EntryCreate( int numObjects );
ClimaxP1ChainTemplate*  ClimaxP1CreateChainTemplate( int objectIndex, ClimaxP1Entry* entry );

extern "C" {
// defined in 0x801EDA30 (RwInterface, unit not yet reconstructed): frees the entry and its objects.
void fn_801EDA30( ClimaxP1Entry* entry );
// defined in 0x801EDABC (RwInterface, unit not yet reconstructed): destroys a chain item and its
// children.
void fn_801EDABC( ClimaxP1ChainItem* item );
}

#endif
