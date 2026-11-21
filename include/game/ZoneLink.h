#ifndef GAME_ZONELINK_H
#define GAME_ZONELINK_H

#include <revolution/types.h>
#include <climax/unk_801A92A8.h>
#include <game/CGenericSpawner.h>

class unk_ZoneTarget;

/*
 * Placeholder bases of ZoneLink, reconstructed only as far as its own methods reach. The first base is the
 * CTerrainTraversal chain up to 0x44 (vtable chain not yet modelled); the second covers 0x44..0x344 and holds
 * unk_8001777C at 0x44 and unk_80039E40 at 0x200. Names invented.
 */
class unk_ZoneLinkHead {
public:
    u8      field_0x0[0x8];
    u32     field_0x8;
    u8      field_0xC[0x38];
};

class unk_ZoneLinkMid {
public:
    u8      field_0x0[0x130];
    void*   field_0x130;                // 0x174 of ZoneLink, the resource the entry table points at
    u8      field_0x134[0xEC];
    unk_801A92A8    field_0x264;        // 0x264 of ZoneLink, message id; fn_8018309C resolves it into field_0x26c
    unk_ZoneTarget* field_0x26c;        // 0x26C of ZoneLink, the target fn_8018309C resolved, cached
    u8      gap_0x270[0x68];
    u8      field_0x2D8;                // 0x2D8 of ZoneLink
    u8      gap_0x2D9[0x5B];
    u8      field_0x334;                // 0x334 of ZoneLink
    u8      gap_0x335[0xF];
};

/* One of the four entries at 0x344 of ZoneLink, 0x14 bytes each. Name and member names invented. */
struct unk_ZoneLinkEntry {
    void*   field_0x0;
    void*   field_0x4;
    void*   field_0x8;
    void*   field_0xC;
    u8      field_0x10[0x4];
};

/* Resource-manager helper at 0x8009E894 (unit not yet reconstructed); the first argument is the unk_8001777C base. */
extern "C" void fn_8009E894( unk_ZoneLinkMid* base, void* old, u32 value );

/*
 * A zone link, registered as "ZoneLink" (size 0x3CC, constructor 0x8002BC7C, factory 0x8002D208).
 * Only the methods reconstructed so far are declared; the rest of the class is open. Members at 0x264, 0x26C,
 * 0x2D8, 0x334 and 0x3A8 are read by game/unk_8002CD08 (the first four sit inside unk_ZoneLinkMid).
 */
class ZoneLink : public unk_ZoneLinkHead, public unk_ZoneLinkMid, public unk_SpawnerResource {
public:
    void            fn_8002C79C();
    void*           fn_8002C824();
    void*           fn_8002C82C();
    void*           fn_8002C850();
    u32             fn_8002C874();
    u8              fn_8002C87C();
    void            fn_8002C8A0( u32 value );

    unk_ZoneLinkEntry   entries[4];     // 0x344
    s32                 entryIndex;     // 0x394, -1 when no entry is current
    u32                 field_0x398;    // 0x398
    u8                  field_0x39C;    // 0x39C, the last value passed to fn_8002C8A0
    u8                  field_0x39D[0xB];
    unk_801A92A8        field_0x3A8;    // 0x3A8, traversal message id; its registrations count is at 0x3AE
    u8                  field_0x3B0[0x14];
    void*               field_0x3C4;    // 0x3C4, request released by fn_8002C79C
    void*               field_0x3C8;    // 0x3C8, request released by fn_8002C79C
};

/* The object fn_8018309C returns for a ZoneLink. Name invented; only fields 0x54 to 0xE4 are recovered. */
class unk_ZoneTarget {
public:
    u8                  gap_0x0[0x54];
    u8                  field_0x54;     // 0x54
    /* Pointer tables read by the accessors of game/unk_8002CD08 (element type not recovered).
       The 2x2 tables are index x flag; the 3-entry runs are indexed by element; the singles
       are per flag or per alternative. */
    void*               field_0x58[2][2];   // 0x58
    void*               field_0x68[2][2];   // 0x68
    void*               field_0x78[2][2];   // 0x78
    void*               field_0x88[2][2];   // 0x88
    void*               field_0x98;         // 0x98, flag 0
    void*               field_0x9C;         // 0x9C, flag 0
    void*               field_0xA0[3];      // 0xA0, flag 0
    void*               field_0xAC[3];      // 0xAC, flag 0
    void*               field_0xB8;         // 0xB8, flag 1
    void*               field_0xBC;         // 0xBC, flag 1
    void*               field_0xC0[3];      // 0xC0, flag 1
    void*               field_0xCC[3];      // 0xCC, flag 1
    void*               field_0xD8;         // 0xD8, alternative of unk_8002CD08's D04C
    void*               field_0xDC;         // 0xDC, alternative of CFE4
    void*               field_0xE0;         // 0xE0, alternative of CF7C
};

extern "C" unk_ZoneTarget* fn_8018309C( ZoneLink* link );

#endif
