#ifndef GAME_UNK_8000F80C_H
#define GAME_UNK_8000F80C_H

#include <revolution/types.h>

namespace RWS {
namespace SHUtils {
// Defined at 0x801B605C, unit not yet reconstructed.
u32 GetStringHashForceLower( const char* str, int length );
}
}

/*
 * The object a unk_8000F80C reads its attributes from. Name invented; only the members the
 * constructor reads are known.
 */
class unk_8000F80CSource {
public:
    const char*             name;       // 0x0
    u8                      field_0x4[0x1C];
    struct unk_8000F80CData* data;      // 0x20
};

// Name invented. Its halfword at 0xE is copied into the constructed object.
struct unk_8000F80CData {
    u8                      field_0x0[0xE];
    s16                     field_0xE;
};

// The value of an attribute. Name invented; the constructor reads its word at 0x4.
struct unk_8000F80CValue {
    u8                      field_0x0[4];
    s32                     flags;      // 0x4
};

// Defined in units not yet reconstructed. fn_80188218 returns the holder of the source's
// attribute, or NULL when there is none; fn_80189D10 returns the slot that holds its value.
struct unk_8000F80CHolder;
extern "C" {
unk_8000F80CHolder*         fn_80188218( const unk_8000F80CSource* source, const char* name );
unk_8000F80CValue**         fn_80189D10( unk_8000F80CHolder* holder, const char* name );
}

// The registry at lbl_8045A6BC: 12 zero-initialised bytes that fn_80012904 keeps as an array of
// (key, value) pairs. Names invented.
struct unk_8000F80CEntry {
    const void*             key;
    void*                   value;
};

struct unk_8000F80CRegistry {
    unk_8000F80CEntry*      entries;    // 0x0
    s32                     count;      // 0x4
    s32                     field_0x8;  // 0x8
};

extern unk_8000F80CRegistry lbl_8045A6BC;   // defined in a unit not yet reconstructed
extern "C" void fn_80012904( unk_8000F80CRegistry* registry, const void* key, void* value );

/*
 * A base class of the classes that fn_8000FC5C and fn_800110FC construct. Name invented: no
 * string names the class.
 */
class unk_8000F80C {
public:
                    unk_8000F80C( unk_8000F80CSource* source, u32 arg, void* owner );  // 0x8000F80C
    const char*     GetName() const;                    // 0x8000F8E8
    const u32&      GetNameHash() const;                // 0x8000F8F0

    const char*     name;           // 0x0
    u32             nameHash;       // 0x4
    u32             field_0x8;      // 0x8, the constructor's second argument
    s32             field_0xC;      // 0xC, the halfword of the source's data
    s32             flags;          // 0x10, 2 unless the source has a "Flags" attribute
    void*           field_0x14;     // 0x14, the constructor's third argument
};

#endif
