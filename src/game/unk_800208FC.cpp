// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800208FC.h>
#include <rwsdk/tool/anim/rtanim.h>

/* Returns the first node from node on whose key is key, or NULL. Name invented. */
static inline unk_CacheNode* unk_FindCacheNode( unk_CacheNode* node, RtAnimInterpolator* key ) {
    for ( ; node != NULL; node = node->next ) {
        if ( node->key == key ) {
            return node;
        }
    }
    return NULL;
}

/* Unlinks node from the active list of cache. Name invented. */
static inline void unk_UnlinkActive( unk_CacheList* cache, unk_CacheNode* node ) {
    unk_CacheNode* prev = node->prev;
    unk_CacheNode* next = node->next;
    if ( prev == NULL ) {
        cache->activeHead = next;
    } else {
        prev->next = next;
    }
    if ( next == NULL ) {
        cache->activeTail = prev;
    } else {
        next->prev = prev;
    }
    node->prev = NULL;
    node->next = NULL;
    cache->activeCount--;
}

/* Appends node to the cached list of cache. Name invented. */
static inline void unk_AppendCached( unk_CacheList* cache, unk_CacheNode* node ) {
    node->prev = cache->cachedTail;
    node->next = NULL;
    if ( cache->cachedTail != NULL ) {
        cache->cachedTail->next = node;
    }
    if ( cache->cachedHead == NULL ) {
        cache->cachedHead = node;
    }
    cache->cachedTail = node;
    cache->cachedCount++;
}

/*
====================
unk_803E0D80::~unk_803E0D80
====================
*/
// 0x800208FC
// Wall (control flow): the retail body tests this twice in a row and reuses cr1 for the destroy after the delete, which the single-pass source does not reproduce; 85.75% exact of the 0x1C8 bytes.
unk_803E0D80::~unk_803E0D80() {
    RtAnimInterpolator* interp = m_interp;
    if ( interp != NULL ) {
        unk_CacheList* cache = m_owner->m_cache;
        unk_CacheNode* node = unk_FindCacheNode( cache->activeHead, interp );
        if ( node != NULL ) {
            if ( cache->activeCount + cache->cachedCount <= 20 ) {
                unk_UnlinkActive( cache, node );
                unk_AppendCached( cache, node );
            } else {
                unk_UnlinkActive( cache, node );
                delete node;
                RtAnimInterpolatorDestroy( interp );
            }
        }
        m_interp = NULL;
    }
}

/*
====================
unk_80054F0C::~unk_80054F0C
====================
*/
// 0x80020AC4
unk_80054F0C::~unk_80054F0C() {
}
