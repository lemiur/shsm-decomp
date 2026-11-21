#ifndef GAME_ZONELINK_H
#define GAME_ZONELINK_H

#include <revolution/types.h>
#include <game/CGenericSpawner.h>

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
    u8      field_0x134[0x1CC];
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
 * Only the methods reconstructed so far are declared; the rest of the class is open.
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
    u8                  field_0x39D[0x27];
    void*               field_0x3C4;    // 0x3C4, request released by fn_8002C79C
    void*               field_0x3C8;    // 0x3C8, request released by fn_8002C79C
};

#endif
