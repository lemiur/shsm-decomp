#ifndef GAME_UNK_80012330_H
#define GAME_UNK_80012330_H

// unit name unconfirmed: header name invented.

#include <revolution/types.h>

/*
 * A reference-counted block that owns one RenderWare allocation, released when its count
 * falls to zero. Role name invented; only the members the release path uses are known.
 */
struct unk_RefBlock {
    u8      field_0x0[0x30];
    s32     refCount;   // 0x30
    void*   rwData;     // 0x34: allocated and freed through RwMalloc / RwFree
};

extern "C" {
void fn_80012330( unk_RefBlock* block, void* owner );
}

#endif
