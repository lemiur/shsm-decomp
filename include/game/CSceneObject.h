#ifndef GAME_CSCENEOBJECT_H
#define GAME_CSCENEOBJECT_H

#include <climax/Memory.h>
#include <game/unk_8009D2E4.h>
#include <game/unk_ClumpBounds.h>

/*
 * A behaviour with a clump whose atomics' bounding spheres it keeps refitted: it takes them on
 * creation and, on each frame message (lbl_8052A788), refits them while lbl_80491B38.field_0x1C is
 * set. Base of CCollidableSceneObject and CScalingObject. Registered as "CSceneObject", vtable
 * lbl_803E9510, size 0x148, constructor 0x800A5670, destructor 0x800A56E8. fn_8009CB1C also
 * overrides the slot of the unk_80024158 base and the destructor those of the other bases, so
 * MWCC adds the thunks. Member names invented.
 */
class CSceneObject : public unk_8009D2E4 {
public:
                    CSceneObject( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual         ~CSceneObject();  // override

    static CAttributeHandler* fn_800A5860( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSceneObject( packet );
    }

    unk_ClumpBounds*    bounds;     // 0x144, the bounds of the clump of unk_80024158
};

extern u32 lbl_80529810;            // class-name hash of "CSceneObject"
extern unk_801A8F6C lbl_80529814;   // type-registry entry of CSceneObject

#endif
