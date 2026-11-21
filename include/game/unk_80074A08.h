#ifndef GAME_UNK_80074A08_H
#define GAME_UNK_80074A08_H

#include <revolution/types.h>
#include <climax/AttributeHandler.h>
#include <climax/Clump.h>
#include <game/CGenericSpawner.h>
#include <game/unk_NamedEntryPlayer.h>
#include <rwsdk/plugin/random/rprandom.h>

// unit name unconfirmed: no string or class-name evidence names this unit. Its records are
// invented (see docs/exceptions.md, open items).

struct unk_RpWorld;  // RenderWare world (RpWorld): only handled through pointers here

/* One link of a clump list, 0xC bytes: the clump and its neighbours. */
struct unk_ClumpNode {
                    unk_ClumpNode( unk_RpClump* clump_ ) : clump( clump_ ), next( NULL ), prev( NULL ) {}

    unk_RpClump*    clump;      // 0x0
    unk_ClumpNode*  next;       // 0x4
    unk_ClumpNode*  prev;       // 0x8
};

/* A request to file one clump, 0x10 bytes. Field names invented. */
struct unk_ClumpEntry {
                    unk_ClumpEntry( int slotA_, int slotB_ ) : field_0x0( 0 ), slotA( slotA_ ), slotB( slotB_ ), clump( NULL ) {}

    int             field_0x0;  // 0x0, set while the request waits to be filed
    int             slotA;      // 0x4, index of a slot
    int             slotB;      // 0x8, index of a slot
    unk_RpClump*    clump;      // 0xC, the clump filed for the request
};

/* A link of the request list, 0xC bytes: the request and its neighbours. */
struct unk_ClumpEntryNode {
                    unk_ClumpEntryNode( unk_ClumpEntry* entry_ ) : entry( entry_ ), next( NULL ), prev( NULL ) {}

    unk_ClumpEntry*     entry;  // 0x0
    unk_ClumpEntryNode* next;   // 0x4
    unk_ClumpEntryNode* prev;   // 0x8
};

/* One slot of the roster, 0x18 bytes. Field names invented. */
struct unk_ClumpSlot {
    u8                              active;     // 0x0
    u8                              loaded;     // 0x1
    u8                              field_0x2[ 6 ];
    void*                           resource;   // 0x8, the resource loaded for the slot
    RWS::AssetTracker::CAssetList*  assets;     // 0xC, the asset list the resource is held by
    unk_RpClump*                    clump;      // 0x10
    u8                              field_0x14[ 4 ];
};

/* The source record the roster reads its requests from. Names and flag roles invented. */
struct unk_ClumpSource {
    u8              flag0;      // 0x0, requests slot 0 for the first list
    u8              flag1;      // 0x1, requests slot 0 for the second list
    u8              flag2;      // 0x2, names a slot for the first list
    u8              flag3;      // 0x3, names a slot for the second list
    u8              flag4;      // 0x4, names a slot for the first list
    u8              flag5;      // 0x5, names a slot for the second list
    u8              field_0x6[ 2 ];
    const char*     name2;      // 0x8
    const char*     name3;      // 0xC
    const char*     name4;      // 0x10
    const char*     name5;      // 0x14
};

/* The clump roster, 0x104 bytes. Class name invented. */
struct unk_ClumpRoster : public unk_SpawnerResource {
    int             field_0x0;  // 0x0, the number of requests the roster files
    unk_ClumpSource* source;    // 0x4, the source record the requests are read from
    int             field_0x8;  // 0x8
    int             field_0xC;  // 0xC, count of the request list
    unk_ClumpEntryNode* field_0x10; // 0x10, head of the request list
    unk_ClumpEntryNode* field_0x14; // 0x14, tail of the request list
    int             field_0x18; // 0x18
    int             count;      // 0x1C, count of the clump list
    unk_ClumpNode*  head;       // 0x20
    unk_ClumpNode*  tail;       // 0x24
    unk_ClumpSlot   slots[ 9 ]; // 0x28
    int             loadedCount; // 0x100
};

/* A part handed to fn_80073FB8: a clump with a key and a flag. Names invented. */
struct unk_ClumpPart {
    unk_RpClump*    clump;      // 0x0
    int*            key;        // 0x4
    int             flag;       // 0x8
};

extern "C" {

// defined in RenderWare, unit not yet reconstructed: the world a clump is in, or NULL
unk_RpWorld* RpClumpGetWorld( const unk_RpClump* clump );

// defined in RenderWare, unit not yet reconstructed: takes clump out of world
unk_RpWorld* RpWorldRemoveClump( unk_RpWorld* world, unk_RpClump* clump );

// defined in 0x80073FB8, unit not yet reconstructed: its parameters are read from its call in
// fn_800751A8 and it returns a clump
unk_RpClump* fn_80073FB8( unk_ClumpPart* parts, int count, unk_RpClump* clump, int flag );

// defined in 0x8014F114, unit not yet reconstructed: looks a name up in the player's table, writes
// the index it finds to index and returns whether it found the name
bool    fn_8014F114( unk_NamedEntryPlayer* player, const char* name, int* index );

void fn_80074A08( unk_ClumpRoster* roster );
void fn_800751A8( unk_ClumpRoster* roster, unk_ClumpEntry* entry );
void fn_80075264( unk_ClumpRoster* roster, unk_ClumpEntry* entry );
void fn_80075324( unk_ClumpRoster* roster, int removeFromWorld );

}

// the name table of the nine slots, defined elsewhere (0x803E6568, ten entries)
extern const char* lbl_803E6568[ 10 ];

// the index limits the source names are clamped to, defined elsewhere (0x80527068 to 0x80527084)
extern int lbl_80527068;
extern int lbl_8052706C;
extern int lbl_80527070;
extern int lbl_80527074;
extern int lbl_80527078;
extern int lbl_8052707C;
extern int lbl_80527080;
extern int lbl_80527084;

#endif
