#ifndef GAME_ATOMICTAGS_H
#define GAME_ATOMICTAGS_H

#include <revolution/types.h>
#include <rwsdk/world/baclump.h>

/* Finds the atomics of a clump by their tag type. Class name from the mangled names of its functions. */
class AtomicTags {
public:
    /* The atomic tag types, in table order. The identifiers are invented from the display names
       the tag function returns (the strings at lbl_803DFB00 and in the string pool). */
    enum E_AtomicType {
        Face,           // 0x0, "Face"
        Other,          // 0x1, "Other"
        Ropes,          // 0x2, "Ropes"
        Weapon,         // 0x3, "Weapon"
        Hair,           // 0x4, "Hair"
        NiceFX,         // 0x5, "NiceFX"
        Phone,          // 0x6, "Phone"
        ForceAlpha,     // 0x7, "ForceAlpha"
        PreEffects,     // 0x8, "PreEffects"
        Body,           // 0x9, "Body"
        Head,           // 0xA, "Head"
        Glasses,        // 0xB, "Glasses"
        Prop1,          // 0xC, "Prop1"
        Prop2,          // 0xD, "Prop2"
        Prop3,          // 0xE, "Prop3"
        ShadowMesh,     // 0xF, "ShadowMesh"
        Invalid         // 0x10, "Invalid"
    };

    /* The search record FindAtomicByType passes through the clump walk. Name invented. */
    struct TypeSearch {
        E_AtomicType    type;       // 0x0
        RpAtomic*       found;      // 0x4
    };

    /* One link of the list FindAllAtomicsByTypeCB builds. Name invented. */
    struct AtomicNode {
        RpAtomic*       atomic;     // 0x0
        AtomicNode*     next;       // 0x4
    };

    /* The search record of the list walk; the caller starts the list at head. Name invented. */
    struct TypeListSearch {
        E_AtomicType    type;       // 0x0
        AtomicNode*     head;       // 0x4
    };

    static RpAtomic*    FindAtomicByType( RpClump* clump, E_AtomicType type );
    static RpAtomic*    FindAtomicByTypeCB( RpAtomic* atomic, void* data );
    static RpAtomic*    FindAllAtomicsByTypeCB( RpAtomic* atomic, void* data );
};

// the tag type of an atomic (0x8000C32C)
extern "C" AtomicTags::E_AtomicType fn_8000C32C( RpAtomic* atomic );

// the display name of a tag type (0x8000C3E4)
extern "C" const char* fn_8000C3E4( AtomicTags::E_AtomicType type );

// the name table at 0x803DFB00: "atomicType" at 0x0, the display names at 0xC, 0x18 and 0x24, and
// the default text at 0x30, each on a 12-byte stride
extern char lbl_803DFB00[0x44];

#endif
