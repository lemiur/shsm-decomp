#ifndef GAME_UNK_8000D090_H
#define GAME_UNK_8000D090_H

// unit name unconfirmed: header name invented. The class is named for its constructor's address
// (0x8000D090); no string names it. Member and parameter names are invented.

#include <climax/Physics.h>

/* The shape definition a body is built from (passed to fn_8030E904). Names invented. */
struct unk_RwpShapeDef {
    u8                  field_0x0[0x50];
    unk_RwV3d           lo;             // 0x50, the box's low corner
    unk_RwV3d           hi;             // 0x5C, the box's high corner
};

/* The body definition copied into the object at 0x8C (0x6C bytes). Names invented. */
struct unk_RwpBodyDef {
    unk_RwV3d           field_0x0;      // 0x0
    float               field_0xC;      // 0xC
    u32                 field_0x10;     // 0x10
    u32                 field_0x14;     // 0x14
    u32                 field_0x18;     // 0x18
    u8                  field_0x1C;     // 0x1C
    u8                  field_0x1D;     // 0x1D
    u32                 field_0x20;     // 0x20
    float               field_0x24;     // 0x24
    float               field_0x28;     // 0x28
    float               field_0x2C;     // 0x2C
    float               field_0x30;     // 0x30
    u32                 field_0x34;     // 0x34
    u32                 field_0x38;     // 0x38
    u32                 field_0x3C;     // 0x3C
    u8                  field_0x40;     // 0x40
    u8                  field_0x41;     // 0x41
    u32                 field_0x44;     // 0x44
    float               field_0x48;     // 0x48
    float               field_0x4C;     // 0x4C
    float               field_0x50;     // 0x50
    float               field_0x54;     // 0x54
    u32                 field_0x58;     // 0x58
    u32                 field_0x5C;     // 0x5C
    u32                 field_0x60;     // 0x60
    u8                  field_0x64;     // 0x64
    u8                  field_0x65;     // 0x65
    u32                 field_0x68;     // 0x68
};

/* The space builder at 0x8 of the object: its methods are called with it first. Name invented. */
struct unk_RwpSpaceParams {
    u8                  field_0x0[0x7C];
};

/* A box of the space: the low corner, then the high corner at 0x10. Names invented. */
struct unk_SpaceBox {
    unk_RwV3d           min;            // 0x0
    float               field_0xC;      // 0xC
    unk_RwV3d           max;            // 0x10
};

/*
 * A physics body with its shape, placed in a new space and registered with the physics manager.
 * Role name invented; its constructor is at 0x8000D090 (the object is 0x10C bytes).
 */
class unk_8000D090 {
public:
    unk_8000D090( const unk_RwpShapeDef* shapeDef, const unk_RwpBodyDef& bodyDef, void* arg6, void* arg7,
                  const float* sizes, const unk_RwV3d* position );
                ~unk_8000D090();

    unk_RwpSpace*           space;          // 0x0
    unk_RwpBitTable*        bits;           // 0x4
    unk_RwpSpaceParams      params;         // 0x8
    const unk_RwpShapeDef*  shapeDef;       // 0x84
    unk_RwpShape*           shape;          // 0x88
    unk_RwpBodyDef          body;           // 0x8C
    void*                   field_0xF8;     // 0xF8
    void*                   field_0xFC;     // 0xFC
    unk_RwV3d               position;       // 0x100
};

#endif
