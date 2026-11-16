#ifndef GAME_CPRECACHE_H
#define GAME_CPRECACHE_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/CGenericSpawner.h>
#include <game/unk_RespotManager.h>

// defined in 0x801B2380 and 0x801B0588, unit not yet reconstructed: set the priority of a cache
// request (not below 0), and attach a name to it
void    fn_801B2380( unk_ResourceManager* manager, void* handle, float priority );
void    fn_801B0588( unk_ResourceManager* manager, void* handle, const char* name );

/*
 * Caches the asset its packet names, and the texture dictionary that goes with it, while a
 * save-data value (fn_80206348) is set: at load when asked to, and on its cache message; its
 * uncache message drops both requests. vtable __vt__9CPreCache, size 0x64. Member names invented.
 */
class CPreCache : public CBaseBehaviour, public unk_SpawnerResource {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetName,            // text: the asset to cache
        CMD_SetCacheOnLoad,     // integer: nonzero caches at load
        CMD_SetCacheMsg,        // text: the name of the message that caches the asset
        CMD_SetField0x38,       // text: attached to the cache request (fn_801B0588)
        CMD_SetUncacheMsg,      // text: the name of the message that drops the requests
        CMD_SetSaveDataA,       // text: copied to saveDataA
        CMD_SetSaveDataB,       // text: copied to saveDataB
        CMD_SetCacheTxd         // integer: nonzero also caches the texture dictionary
    };

                    CPreCache( const unk_AttributePacket& packet );
    virtual         ~CPreCache();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    virtual void    fn_8000CFD0();
    virtual void    fn_8000CFCC();
    virtual void    fn_80028BB0() {}

    void            fn_80028E38();
    void            UnCache();

    static CAttributeHandler* fn_80029004( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPreCache( packet );
    }

    const char*     name;           // 0x34, points into the attribute packet
    const char*     field_0x38;     // 0x38, points into the attribute packet
    int             field_0x3C;     // 0x3C, the flags of the cache request; never set
    bool            cacheOnLoad;    // 0x40
    bool            cacheTxd;       // 0x41
    unk_801A92A8    cacheMsg;       // 0x44
    unk_801A92A8    uncacheMsg;     // 0x4C
    char*           saveDataA;      // 0x54, owned copy; first argument of fn_80206348
    char*           saveDataB;      // 0x58, owned copy; second argument of fn_80206348
    void*           cacheHandle;    // 0x5C, the request for the asset; NULL when none
    void*           txdHandle;      // 0x60, the request for the texture dictionary; NULL when none
};

extern const char* lbl_80526A00;    // "SaveData", the group of the save-data value
extern u32 lbl_80529248;            // class-name hash of "CPreCache"
extern unk_801A8F6C lbl_8052924C;   // type-registry entry of CPreCache

#endif
