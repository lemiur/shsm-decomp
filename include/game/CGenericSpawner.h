#ifndef GAME_CGENERICSPAWNER_H
#define GAME_CGENERICSPAWNER_H

#include <climax/Memory.h>
#include <game/CBaseSpawner.h>

/* The resource manager behind lbl_8052A5C0 (unit at 0x801B0120 not yet reconstructed). Name invented. */
struct unk_ResourceManager {
    u32 field_0x0;      // 0x0, read by fn_80015F8C; the rest of the object is not recovered
};

extern unk_ResourceManager* lbl_8052A5C0;

/*
 * The owner a resource request is filed under: an empty base of the classes that cache assets
 * (CGenericSpawner, MusicTrigger), so callers pass their converted this. MWCC places it after the
 * other bases, at the offset where the class's own members start. fn_801B05C0 does not read it.
 * Name invented.
 */
class unk_SpawnerResource {
};

/* Loader callback; the first five arguments are unknown and unused here. Name invented. */
typedef void ( *unk_LoadCallback )( u32, u32, u32, u32, u32, void* userdata );

// defined in 0x801B05C0, 0x801B0B8C, 0x801B1E3C and 0x801B2004, unit not yet reconstructed
void*   fn_801B05C0( unk_ResourceManager* manager, const char* name, unk_SpawnerResource* owner, int flags,
                     unk_LoadCallback callback, void* userdata );
void    fn_801B0B8C( unk_ResourceManager* manager, void* handle, unk_SpawnerResource* owner, int flags );
void*   fn_801B1E3C( unk_ResourceManager* manager, const char* name, unk_SpawnerResource* owner, int flags );
void    fn_801B2004( unk_ResourceManager* manager, void* resource );
// defined in 0x801A8554 and 0x801A89A0, unit not yet reconstructed
void    fn_801A8554( const char* name, void* resource );
void    fn_801A89A0( const char* name );
// defined in 0x800A26D8, unit not yet reconstructed: the first name command in a packet, NULL if none
const char* fn_800A26D8( const unk_AttributePacket& packet );

/*
 * A spawner that caches the asset named by its packet and starts spawning once it is loaded
 * (registered as "CGenericSpawner"; vtable __vt__15CGenericSpawner, size 0x64).
 */
class CGenericSpawner : public CBaseSpawner, public unk_SpawnerResource {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetName     // text: the asset to cache; empty for none
    };

                    CGenericSpawner( const unk_AttributePacket& packet );
    virtual         ~CGenericSpawner();
    virtual void    fn_8000CFD0();
    virtual void    fn_8000CFCC();
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_800C84C8();

    static void     fn_800C8268( u32 arg0, u32 arg1, u32 arg2, u32 arg3, u32 arg4, CGenericSpawner* spawner );

    static CAttributeHandler* fn_800C8548( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CGenericSpawner( packet );
    }

    /* Member names invented. */
    const unk_AttributePacket*  packet;         // 0x4C, the packet that created the spawner
    const char*                 name;           // 0x50, NULL: nothing to cache
    void*                       cacheHandle;    // 0x54, the request that loads name; NULL when none
    u32                         spawnArg;       // 0x58
    void*                       data;           // 0x5C
    bool                        wasEnabled;     // 0x60, enabled, saved while the asset loads
};

extern u32 lbl_80529AC0;            // class-name hash of "CGenericSpawner"
extern unk_801A8F6C lbl_80529AC4;   // type-registry entry of CGenericSpawner

#endif
