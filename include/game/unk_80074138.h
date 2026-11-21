#ifndef GAME_UNK_80074138_H
#define GAME_UNK_80074138_H

#include <revolution/types.h>
#include <climax/AttributeHandler.h>
#include <climax/Clump.h>
#include <climax/unk_List.h>
#include <game/CGenericSpawner.h>

// RenderWare (C linkage), defined in 0x802DB370 and 0x802DB1FC, unit not yet reconstructed
struct unk_RpWorld;
extern "C" {
unk_RpWorld*    RpClumpGetWorld( unk_RpClump* clump );
unk_RpWorld*    RpWorldRemoveClump( unk_RpWorld* world, unk_RpClump* clump );
}

/*
 * An item of the first list (its node holds a pointer to it). Name invented.
 */
struct unk_PendingItem {
    u32                 field_0x00;     // 0x00, nonzero: the item moves on to the second list
    u32                 field_0x04;     // 0x04
    u32                 field_0x08;     // 0x08
    unk_RpClump*        field_0x0C;     // 0x0C, the clump the second list holds
};

/*
 * One of the nine slots at +0x28 of unk_80074138 (0x18 bytes each). Name invented.
 */
struct unk_SlotEntry {
    u32                             field_0x00;     // 0x00, not read by the unit
    u8                              field_0x01;     // 0x01, flag
    u8                              field_0x02[ 3 ];
    void*                           field_0x04;     // 0x04, released through fn_801B2004
    void*                           field_0x08;     // 0x08, released through fn_801B0B8C
    RWS::AssetTracker::CAssetList*  field_0x0C;     // 0x0C, DecRefCount target
    u32                             field_0x10;     // 0x10
};

// defined in 0x80074A08, unit not yet reconstructed (called last by the constructor)
extern "C" void fn_80074A08( void* self );

/*
 * Name invented. The class holds two lists and a table of nine slots; its constructor is fn_80074138
 * and its destructor is fn_800744B0. It is an unk_SpawnerResource: fn_801B0B8C takes it as the owner.
 */
class unk_80074138 : public unk_SpawnerResource {
public:
    unk_80074138( int key, int value );
    ~unk_80074138();

    inline void     MoveAToB();

    int                             field_0x00;     // 0x00, compared with the constructor's key argument
    int                             field_0x04;     // 0x04
    unk_List< unk_PendingItem* >    listA;          // 0x08
    unk_List< unk_RpClump* >        listB;          // 0x18
    unk_SlotEntry                   slots[ 9 ];     // 0x28
    int                             field_0x100;    // 0x100, after the slots
};

#endif
