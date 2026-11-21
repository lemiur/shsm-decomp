#ifndef CLIMAX_UNK_INTERPCACHE_H
#define CLIMAX_UNK_INTERPCACHE_H

// Class, member and header names invented: no string names this class, and the constructor
// is not in this unit. The layout comes from the destructor at 0x8002039C.

#include <revolution/types.h>
#include <rwsdk/tool/anim/rtanim.h>
#include <climax/unk_List.h>

typedef unk_List< RtAnimInterpolator* > unk_InterpList;

/* The cache lists of one owner: interpolators in use, and spare nodes that keep their interpolator. */
struct unk_InterpPool {
    unk_InterpList                     active;     // 0x0
    unk_InterpList                     spare;      // 0x10
};

/* The object that holds the pool. Only the pool is known. */
struct unk_InterpOwner {
    u8                                  field_0x0[ 0x48 ];
    unk_InterpPool*                     pool;       // 0x48
};

class unk_InterpCache {
public:
    virtual                             ~unk_InterpCache();

    unk_InterpOwner*                    owner;          // 0x4
    u8                                  field_0x8[ 0x54 ];
    RtAnimInterpolator*                 interpolator;   // 0x5C
};

#endif
