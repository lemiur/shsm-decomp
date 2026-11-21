#ifndef GAME_CENEMYSPAWN_H
#define GAME_CENEMYSPAWN_H

#include <climax/Heap.h>
#include <game/CBaseSpawner.h>

/*
 * A spawner that keeps the packet it was created from (registered as "CEnemySpawn", size 0x50; vtable
 * __vt__11CEnemySpawn at lbl_803E30D0, destructor 0x80052018 in a unit not yet reconstructed). The
 * overrides below are defined where their addresses say. Member names invented.
 */
class CEnemySpawn : public CBaseSpawner {
public:
                    CEnemySpawn( const unk_AttributePacket& packet ) {
                        this->packet = &packet;
                    }
    virtual         ~CEnemySpawn();
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );   // 0x80052088
    virtual void    fn_800C84C8();                                       // 0x80052070

    /* The type-registry factory (the registration is in the static initialiser at 0x80052E58). */
    static CAttributeHandler* fn_8005305C( const unk_AttributePacket& packet );

    const unk_AttributePacket*  packet;     // 0x4C, the packet that created the spawner
};

#endif
