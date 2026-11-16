#ifndef GAME_CBASESPAWNER_H
#define GAME_CBASESPAWNER_H

// Header name unconfirmed: the class name is evidenced ("CBaseSpawner"), its unit is not yet reconstructed.

#include <game/CBaseBehaviour.h>

/*
 * The base of the spawners (registered as "CBaseSpawner", vtable lbl_803EBD2C, constructor
 * 0x800C7B38, destructor 0x800C7C58, size 0x4C; unit in 0x800C7688-0x800C8108 not yet
 * reconstructed, also holding fn_800C7E84 and fn_800C7F54 below). Its own virtuals follow
 * CBaseBehaviour's: fn_801A7BFC and fn_8000CFD0 override CAttributeHandler slots,
 * fn_800C84C8 is pure. Member names invented.
 */
class CBaseSpawner : public CBaseBehaviour {
public:
                    CBaseSpawner();
    virtual         ~CBaseSpawner();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );
    virtual void    fn_800C84C8() = 0;
    virtual void    fn_8000CFD0();

    void            fn_800C7C04();

    u32             field_0x34;
    u32             field_0x38;
    u32             field_0x3C;
    bool            enabled;        // 0x40, set by the constructor
    unk_801A92A8    field_0x44;
};

// defined in 0x800C7E84, unit not yet reconstructed: queues a deferred request of owner key
// packet (a copy of the size bytes at data is kept)
void fn_800C7E84( CZone* zone, const char* name, const unk_AttributePacket* packet, u32 arg, void* data, int size );
// defined in 0x800C7F54, unit not yet reconstructed: drops the queued requests of key
void fn_800C7F54( const unk_AttributePacket* key );

#endif
