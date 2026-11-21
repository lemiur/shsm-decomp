#ifndef GAME_UNK_800298AC_H
#define GAME_UNK_800298AC_H

#include <revolution/types.h>
#include <climax/Frame.h>

// Vectors of a placement source (struct and member names invented; offsets from the retail reads).
struct unk_PlacementSource {
    u8          field_0x0[0xA];
    u16         field_0xA;          // 0xA, passed on to fn_801308C8
    u8          field_0xC[0x4];
    unk_RwV3d   right;              // 0x10
    unk_RwV3d   up;                 // 0x1C
    unk_RwV3d   at;                 // 0x28
    unk_RwV3d   pos;                // 0x34
};

// Object that owns a placement source and the handle created on first use (name invented).
struct unk_Placement {
    unk_PlacementSource* source;    // 0x0
    u8                   field_0x4[0x10];
    void*                handle;    // 0x14, zero until fn_8002A684 creates it
};

// Spawn point file layout (struct and member names invented; offsets from the retail reads).
struct unk_SpawnPointHeader {
    u16 field_0x0;
    u16 groupCount;                 // 0x2
};

struct unk_SpawnPointEntry {        // 0x14 bytes
    u8  field_0x0[4];
    u16 nameIndex;                  // 0x4, index into the file's name offset table
    u8  field_0x6[0xE];
};

struct unk_SpawnPointGroup {
    unk_SpawnPointHeader* header;   // 0x0
    unk_SpawnPointEntry*  entries;  // 0x4
};

struct unk_SpawnPointFile {
    u8  field_0x0[4];
    unk_SpawnPointHeader* header;   // 0x4
    unk_SpawnPointGroup*  groups;   // 0x8
    u32*  nameOffsets;              // 0xC, big-endian offsets; each name sits at this base plus its offset
};

extern "C" {
void fn_800298AC();
void fn_8002A584();
void* fn_8002A684( unk_Placement* self, void* parent );
unk_SpawnPointEntry* fn_8002A758( unk_SpawnPointGroup* group, unk_SpawnPointFile* file, int nameIndex );
void fn_8002A828();
void* fn_8002A964( void* self, int flags );
}

#endif
