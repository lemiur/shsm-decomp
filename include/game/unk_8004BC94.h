#ifndef GAME_UNK_8004BC94_H
#define GAME_UNK_8004BC94_H

#include <rwsdk/tool/anim/rtanim.h>
#include <revolution/types.h>

/*
 * unit name unconfirmed: the class is named after its constructor fn_8004BC94 (it stores the
 * vtable lbl_803E0C60); no string names it. Member names invented.
 */

/* The base vtable (lbl_803E0D80), owned by the data unit that holds the vtables. */
extern void* lbl_803E0D80[];

/* A node of an interpolator list: the interpolator is the key, the first word. */
struct unk_InterpNode {
    RtAnimInterpolator* interpolator;   // 0x00
    unk_InterpNode*     next;           // 0x04
    unk_InterpNode*     prev;           // 0x08
};

/*
 * A pool of interpolator nodes: the active list (count, head, tail) and the cache of released
 * nodes (cacheCount, cacheHead, cacheTail). Active and cached nodes together hold at most 20.
 */
struct unk_InterpList {
    u8                  field_0x0[0x4];
    s32                 count;          // 0x04
    unk_InterpNode*     head;           // 0x08
    unk_InterpNode*     tail;           // 0x0C
    u8                  field_0x10[0x4];
    s32                 cacheCount;     // 0x14
    unk_InterpNode*     cacheHead;      // 0x18
    unk_InterpNode*     cacheTail;      // 0x1C
};

struct unk_InterpSystem {
    u8                  field_0x0[0x48];
    unk_InterpList*     interpList;     // 0x48
};

class unk_8004BC94 {
public:
    virtual ~unk_8004BC94();

    unk_InterpSystem*   system;         // 0x04
    u8                  field_0x8[0x54];
    RtAnimInterpolator* interpolator;   // 0x5C
};

#endif
