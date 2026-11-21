#ifndef GAME_UNK_80120188_H
#define GAME_UNK_80120188_H

// unit name unconfirmed: header and type names invented. The class is named after its
// constructor fn_80120188, which is reconstructed in its own unit (not yet).

#include <revolution/types.h>
#include <game/unk_HUDRequest.h>

struct unk_EndList;

/*
 * A timed object of 0x28 bytes, one end of a pair: each end holds the other at 0x14 and the list
 * it was built for at 0x8 (not counted). fn_80120188 builds it from a distance, a list, a kind and
 * a lifetime in seconds, stored in milliseconds at 0x20. Its base class's vtable (0x803F6490) is
 * stored first and overwritten by this class's vtable (0x803F5EB8). The base is not reconstructed.
 * Names invented.
 */
class unk_80120188 {
public:
    virtual         ~unk_80120188();    // vtable 0x8: deleting destructor, called with flag 1
    virtual void    fn_80120264();      // vtable 0xC
    virtual void    fn_80133FB0();      // vtable 0x10, inherited from the base
    virtual void    fn_80134098();      // vtable 0x14, inherited from the base
    virtual void    fn_80120428();      // vtable 0x18
    virtual void    fn_801204B4();      // vtable 0x1C

    void            AddRef() {
                        refCount++;
                    }
    void            Release() {
                        if ( --refCount == 0 ) {
                            delete this;
                        }
                    }

    int             refCount;       // 0x4, references held by unk_RefPtr
    unk_EndList*   list;           // 0x8, the list the object was built for (not counted)
    u32             field_0xc;      // 0xC, zeroed by fn_80120188
    u32             field_0x10;     // 0x10, zeroed by fn_80120188
    unk_80120188*   partner;        // 0x14, the other end of the pair (not counted)
    float           dist;           // 0x18
    u32             field_0x1c;     // 0x1C, zeroed by fn_80120188
    u32             lifetimeMs;     // 0x20, lifetime in milliseconds
    int             kind;           // 0x24
};

// Builds the object in place and returns it. Constructor fn_80120188, reconstructed in its own unit.
extern "C" {
unk_80120188*   fn_80120188( unk_80120188* self, int kind, unk_EndList* list, float dist, float seconds );
}

#endif
