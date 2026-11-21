#ifndef GAME_UNK_REGISTRY_H
#define GAME_UNK_REGISTRY_H

// Unit name unconfirmed: the registry, its entry and its node have no string evidence, so their
// names are invented; the layouts are the retail ones (read from fn_800ABADC, fn_800ABBDC,
// fn_800ABD5C, fn_800ABDA0 and fn_800ABDE4).

#include <revolution/types.h>
#include <climax/unk_List.h>

struct unk_RegistryEntry;

/* Release callback of an entry: receives the entry and its handle. */
typedef void ( *unk_RegistryReleaseFn )( unk_RegistryEntry* entry, void* handle );

/* One registration: a name, the handle it stands for, a reference count and a release callback. */
struct unk_RegistryEntry {
    char                name[ 0x100 ];  // 0x000, compared without case by the name lookup
    void*               handle;         // 0x100, returned by the name lookup; zero once released
    s32                 refCount;       // 0x104, bumped by fn_800ABD5C, dropped by fn_800ABDA0
    unk_RegistryReleaseFn  release;        // 0x108, called with the handle when the entry is removed
};

typedef unk_ListNode< unk_RegistryEntry* > unk_RegistryNode;

/* The registry global: a list of entries, appended at the tail. */
struct unk_Registry {
    u32                 field_0x0;      // 0x0, not used by these functions
    s32                 count;          // 0x4
    unk_RegistryNode*      head;           // 0x8
    unk_RegistryNode*      tail;           // 0xC
};

// Defined by a unit not yet reconstructed (.bss 0x8046EA70, between the objects of
// game/unk_800AC0D8.cpp and game/unk_800AF298.cpp).
extern unk_Registry lbl_8046EA70;

// The first entry of the registry whose handle is the argument, or NULL. Name invented; retail has
// it inlined into fn_800ABD5C and fn_800ABDA0 (the early return and the trailing NULL meet at one
// result register).
inline unk_RegistryEntry* unk_FindRegistryEntry( void* handle ) {
    unk_RegistryNode* node;
    for ( node = lbl_8046EA70.head; node != NULL; node = node->next ) {
        unk_RegistryEntry* entry = node->data;
        if ( handle == entry->handle ) {
            return entry;
        }
    }
    return NULL;
}

#endif
