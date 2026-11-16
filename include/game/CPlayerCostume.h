#ifndef GAME_CPLAYERCOSTUME_H
#define GAME_CPLAYERCOSTUME_H

#include <game/CClumpSwap.h>
#include <game/unk_80491B38.h>

/*
 * The clump swap that dresses the player: while it exists it is the player record's costume
 * (lbl_80491B38.field_0x18). vtable __vt__14CPlayerCostume, size 0x3C.
 */
class CPlayerCostume : public CClumpSwap {
public:
                    CPlayerCostume( const unk_AttributePacket& packet ) : CClumpSwap( packet ) {
                        GetPlayerRecord().field_0x18 = this;
                    }
    virtual         ~CPlayerCostume();

    static CAttributeHandler* fn_80073F30( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CPlayerCostume( packet );
    }
};

extern u32 lbl_80529580;            // class-name hash of "CPlayerCostume"
extern unk_801A8F6C lbl_80529584;   // type-registry entry of CPlayerCostume

#endif
