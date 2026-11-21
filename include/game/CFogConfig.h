#ifndef GAME_CFOGCONFIG_H
#define GAME_CFOGCONFIG_H

// Unit name: the class name string "CFogConfig" in the unit's static initialiser (fn_80092990),
// which registers the class with the registry in fn_801A7A84.

#include <game/CBaseBehaviour.h>
#include <game/unk_80491B38.h>

/* One entry of the particle array (16 bytes, count at 0x44 of CFogConfig). Fields not yet known. */
struct unk_FogParticle {
    float           field_0x0;
    float           field_0x4;
    float           field_0x8;
    float           field_0xC;
};

/*
 * The fog behaviour (vtable lbl_803E88F4, size 0x5C; constructor 0x80091BFC, destructor
 * 0x80091D40). Its own data starts at 0x34, after CBaseBehaviour. Member names are invented
 * where marked; unknown members keep their offset.
 */
class CFogConfig : public CBaseBehaviour {
public:
    /* Attribute commands, one per fog field. Names invented. */
    enum {
        CMD_SetTextureName,
        CMD_SetParticleCount,
        CMD_SetField48,
        CMD_SetField50,
        CMD_SetField4C,
        CMD_SetField54,
        CMD_SetField58,
        CMD_SetFieldA4,
        CMD_SetField94,
        CMD_SetField9C,
        CMD_SetField8C,
        CMD_SetField90,
        CMD_SetField88,
        CMD_SetField60,
        CMD_SetField64,
        CMD_SetField6C,
        CMD_SetField68,
        CMD_SetField70,
        CMD_SetField74
    };

                    CFogConfig( const unk_AttributePacket& packet );    // 0x80091BFC: the factory passes the packet; none of it is read
    virtual         ~CFogConfig();
    virtual void    fn_8009CB1C( unk_Msg& msg );        // 0x80091E28
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );   // 0x80091E80
    virtual void    fn_801A7A88();                      // 0x80092034: finds the texture

    /* The registry's factory: a new fog config. */
    static CAttributeHandler* fn_800929F0( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CFogConfig( packet );
    }

    void            fn_80092074( unk_80491B38* state );  // 0x80092074
    void            fn_80092348( unk_80491B38* state, unk_FogParticle* array, s32 count, float spread, float minimum );  // 0x80092348
    void            fn_80092600( unk_80491B38* state );  // 0x80092600, stub: not reconstructed

    CFogConfig*     nextFogConfig;          // 0x34, the registry's chain of fog configs
    const char*     textureName;            // 0x38, from the attribute packet
    RwRaster*       texture;                // 0x3C, FindRaster( textureName, true )
    unk_FogParticle* particles;             // 0x40, 16-byte entries, count at 0x44
    s32             particleCount;          // 0x44, default 0x5DC
    float           field_0x48;             // default 13.0
    float           field_0x4C;             // default 3.0
    float           field_0x50;             // default 25.0
    float           field_0x54;             // default 0.3
    float           field_0x58;             // default 0.75
    unk_FogParticle* field_0x5C;            // second particle array, count at 0x60
    s32             field_0x60;
    float           field_0x64;             // default 4.0
    float           field_0x68;             // default 2.2
    float           field_0x6C;             // default 20.0
    float           field_0x70;             // default 0.2
    float           field_0x74;             // default 0.5
    float           field_0x78;
    float           field_0x7C;
    float           field_0x80;             // default 1.0
    float           field_0x84;
    float           field_0x88;
    float           field_0x8C;             // default 0.65
    float           field_0x90;             // default 1.0
    float           field_0x94;             // default 0.7
    float           field_0x98;             // default 0.23
    float           field_0x9C;             // default 0.37
    float           field_0xA0;             // the player's frame height (pos.y), read by fn_80092074
    u32             field_0xA4;
};

/*
 * A node of the flagged list headed by lbl_80529998, walked by fn_80092600. Only the two members
 * the walk reaches are named; the rest keep their offsets. Name invented.
 */
struct unk_FlaggedNode {
    u8              field_0x0[0x54];
    unk_FlaggedNode* nextNode;      // 0x54
    u8              field_0x58[6];
    u8              flag;           // 0x5E, nonzero when the node is flagged
};

extern CFogConfig* lbl_80529738;  // head of the registry chain of CFogConfig objects
extern unk_FlaggedNode* lbl_80529998;  // head of the flagged list (defined in CFogConfig.cpp: this unit is its only user)
extern u32 lbl_8052973C;    // the class id of the attribute commands (in fn_80092990)
extern unk_801A8F6C lbl_80529740;   // the registration of "CFogConfig"

#endif
