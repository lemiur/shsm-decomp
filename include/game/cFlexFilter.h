#ifndef GAME_CFLEXFILTER_H
#define GAME_CFLEXFILTER_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <game/CBaseBehaviour.h>

/*
 * The second base of cFlexFilter, 8 bytes at 0x34. Its constructor is 0x801E7A20 and its
 * destructor 0x801E7A24, both in another unit. Retail runs the constructor after CBaseBehaviour's
 * and before the vptr store, and the destructor after the members. Name invented.
 */
class unk_801E7A20 {
public:
                    unk_801E7A20();
                    ~unk_801E7A20();

    u8              field_0x0[0x8];     // contents not recovered
};

/*
 * Behaviour registered under the class name "cFlexFilter" (lbl_803E7E3C). Its factory is
 * fn_80085C64, its size 0x94. Member names invented.
 */
class cFlexFilter : public CBaseBehaviour, public unk_801E7A20 {
public:
                    cFlexFilter( const unk_AttributePacket& packet );
    virtual         ~cFlexFilter();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    void            fn_80085760( float frameTime );     // called on the frame message; name invented

    static CAttributeHandler* fn_80085C64( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) cFlexFilter( packet );
    }

    u32             field_0x3C;         // 0x3C
    float           field_0x40;         // 0x40
    float           field_0x44;         // 0x44
    float           field_0x48;         // 0x48
    float           field_0x4C;         // 0x4C
    float           field_0x50;         // 0x50
    float           field_0x54;         // 0x54
    float           field_0x58;         // 0x58
    float           field_0x5C;         // 0x5C
    float           field_0x60;         // 0x60
    float           field_0x64;         // 0x64
    float           field_0x68;         // 0x68
    float           field_0x6C;         // 0x6C
    u8              field_0x70;         // 0x70
    bool            field_0x71;         // 0x71
    u8              field_0x72[0x2];    // 0x72
    s32             field_0x74;         // 0x74
    float           field_0x78;         // 0x78
    s32             field_0x7C;         // 0x7C
    float           field_0x80;         // 0x80
    unk_801A92A8    field_0x84;         // 0x84
    unk_801A92A8    field_0x8C;         // 0x8C
};

extern unk_801A92A8 lbl_8052A788;   // the frame message (defined by another unit)
extern u32 lbl_80529648;            // class-name hash of "cFlexFilter"
extern unk_801A8F6C lbl_8052964C;   // type-registry entry of cFlexFilter
extern u8 lbl_8046A080[0x400];      // colour ramp, 256 RGBA entries (defined in this unit)

#endif
