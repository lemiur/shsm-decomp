#ifndef GAME_CENEMY_H
#define GAME_CENEMY_H

#include <revolution/types.h>
#include <climax/AttributeHandler.h>
#include <game/unk_80055174.h>
#include <game/unk_8000EC94.h>

/*
 * CEnemy, registered as "CEnemy" with factory 0x80052F3C and size 0xB60. Its constructor is
 * 0x80043950 (another unit). The base is unk_80055174. Members beyond the base are not
 * reconstructed yet, so the rest of the object is padding here.
 */
class CEnemy : public unk_80055174 {
public:
                    CEnemy( const unk_AttributePacket& packet );

    // the override of unk_8003C454's pure slot; its body is in another unit, so the class is concrete
    virtual int     fn_80072D14();  // override

    u8              field_0x788[0xB60 - 0x788];
};

/*
 * The object at 0x80529350 (vtable lbl_803E34F0): one virtual, 0x800530D4, which compares the
 * byte at +0x4 of this with the byte at +0x2F of its argument. Class and parameter names are
 * invented; the argument's class is not reconstructed yet.
 */
class unk_Compare {
public:
    virtual bool    fn_800530D4( const void* other ) const;

    u8              field_0x4;  // 0x4
};

// the factory of CEnemy, the factory registered for "CEnemy"
extern "C" CAttributeHandler* fn_80052F3C( const unk_AttributePacket& packet );
// the factory registered for "CEnemySpawn": its code at 0x8005305C is outside this unit's range
extern "C" CAttributeHandler* fn_8005305C( const unk_AttributePacket& packet );

extern u32 lbl_80529348;        // the registered id of "CEnemy"
extern unk_801A8F6C lbl_8052934C;
extern u32 lbl_80529358;        // the registered id of "CEnemySpawn"
extern unk_801A8F6C lbl_8052935C;

#endif
