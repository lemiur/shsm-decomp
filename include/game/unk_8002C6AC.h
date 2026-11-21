#ifndef GAME_UNK_8002C6AC_H
#define GAME_UNK_8002C6AC_H

#include <revolution/types.h>
#include <game/CGenericSpawner.h>

/*
 * The owner base of unk_8002C6AC: 0x344 bytes whose layout is not recovered. Name invented.
 */
class unk_8002C6ACBase {
public:
    u8 field_0x0[0x344];
};

/* One entry of unk_8002C6AC::slots, 0x14 bytes. Names invented. */
struct unk_8002C6ACSlot {
    const char* name;           // 0x0, the asset the entry names
    u8          field_0x4[0xC];
    bool        active;         // 0x10
};

/*
 * The object that caches the asset its current slot names, and the asset named by its
 * name field, while the current slot is active. Name and layout invented: the class is
 * recovered from its member accesses only.
 */
class unk_8002C6AC : public unk_8002C6ACBase, public unk_SpawnerResource {
public:
    unk_8002C6ACSlot    slots[4];       // 0x344
    int                 index;          // 0x394, the current slot; negative for none
    const char*         name;           // 0x398, the second asset to cache; NULL for none
    u8                  field_0x39C[0x28];
    void*               slotHandle;     // 0x3C4, the request for the slot asset; NULL when none
    void*               nameHandle;     // 0x3C8, the request for the name asset; NULL when none

    /* The name of the current slot, NULL when none. Name invented. */
    const char* CurrentName() const { return index >= 0 ? slots[index].name : NULL; }
};

extern "C" {
void fn_8002C6AC( unk_8002C6AC* self );
}

#endif
