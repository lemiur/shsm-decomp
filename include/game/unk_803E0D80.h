#ifndef GAME_UNK_803E0D80_H
#define GAME_UNK_803E0D80_H

#include <revolution/types.h>
#include <rwsdk/tool/anim/rtanim.h>

/*
 * Class name invented from its vtable address lbl_803E0D80 (no string names it). Its
 * destructor keeps the RtAnimInterpolator in m_interp in an interpolator cache owned by
 * m_owner: an active list and a cached list, linked through node+0x4 (next) and node+0x8 (prev).
 */

/* One entry of the interpolator cache. The key is the interpolator it holds. Names invented. */
struct unk_CacheNode {
    RtAnimInterpolator* key;    // 0x0
    unk_CacheNode*      next;   // 0x4
    unk_CacheNode*      prev;   // 0x8
};

/* The cache's two intrusive lists and their counts. Names invented. */
struct unk_CacheList {
    s32                 field_0x00;     // 0x0
    s32                 activeCount;    // 0x4
    unk_CacheNode*      activeHead;     // 0x8
    unk_CacheNode*      activeTail;     // 0xC
    s32                 field_0x10;     // 0x10
    s32                 cachedCount;    // 0x14
    unk_CacheNode*      cachedHead;     // 0x18
    unk_CacheNode*      cachedTail;     // 0x1C
};

/* The object that owns the cache. Only the cache pointer is recovered. Name invented. */
struct unk_CacheOwner {
    u8                  field_0x00[0x48];   // 0x0
    unk_CacheList*      m_cache;            // 0x48
};

class unk_803E0D80 {
public:
                    ~unk_803E0D80();                    // 0x800208FC
    virtual void    fn_8001B5DC();
    virtual void    fn_8001B778();
    virtual void    fn_800201A8();
    virtual void    fn_8001FB08();
    virtual void    fn_80018D08();
    virtual void    fn_800200AC();
    virtual void    fn_80020120();
    virtual void    fn_8001FB00();
    virtual void    fn_8001FAF8();
    virtual void    fn_80020024();
    virtual void    fn_80020000();
    virtual void    fn_8001FAF4();
    virtual void    fn_8001B764();
    virtual void    fn_8001B664();
    virtual void    fn_8001CD30();
    virtual void    fn_8001C42C();
    virtual void    fn_8001C9B8();
    virtual void    fn_8001EF3C();
    virtual void    fn_8001EEB0();
    virtual void    fn_8001CF74();
    virtual void    fn_8001CF78();

    unk_CacheOwner*     m_owner;            // 0x4
    u8                  field_0x08[0x54];   // 0x8
    RtAnimInterpolator* m_interp;           // 0x5C
};

#endif
