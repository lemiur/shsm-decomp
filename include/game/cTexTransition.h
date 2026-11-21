#ifndef GAME_CTEXTRANSITION_H
#define GAME_CTEXTRANSITION_H

#include <string.h>
#include <climax/Heap.h>
#include <climax/unk_801A92A8.h>
#include <game/CBaseBehaviour.h>
#include <rwsdk/batextur.h>

/*
 * One texture of a transition, with the 256-entry palette it is faded through. Member names
 * invented.
 */
struct unk_TexTransitionNode {
    unk_TexTransitionNode*  next;       // 0x0
    RwTexture*              texture;    // 0x4, NULL when none
    char*                   data;       // 0x8, the name, allocated with new[]
    u16*                    palette;    // 0xC, NULL: the node is skipped
    u32                     field_0x10; // 0x10, zeroed when the node is created
};

/*
 * A behaviour that fades a list of textures through their palettes as its progress field
 * (0x50, 0..1) moves. Registered as "cTexTransition" (lbl_803E86DC and lbl_803E86EC); the
 * unit's Gormiti link-map name (Spawning::CPlayerSpawner) is not the registered class. Layout:
 * CBaseBehaviour's 0x34 bytes, then its own members; vtable lbl_803E8680, size 0x68. Member
 * names invented.
 */
class cTexTransition : public CBaseBehaviour {
public:
                    cTexTransition( const unk_AttributePacket& packet );
    virtual         ~cTexTransition();

    virtual void    fn_801A7A88();                              // overrides CAttributeHandler::fn_801A7A88
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );   // overrides CAttributeHandler::fn_801A73A8
    virtual void    fn_8009CB1C( unk_Msg& msg );                // overrides CBaseBehaviour::fn_8009CB1C

    void            fn_8008EAE0( const char* name );
    void            fn_8008EBC0();
    void            fn_8008ECC0( float elapsed );
    void            fn_8008ED54();

    static CAttributeHandler* fn_8008EEDC( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) cTexTransition( packet );
    }

    unk_801A92A8            field_0x34;
    unk_801A92A8            field_0x3C;
    unk_801A92A8            field_0x44;
    bool                    field_0x4C;     // 0x4C, the transition is running
    bool                    field_0x4D;     // 0x4D, the progress runs backwards
    u8                      field_0x4E;     // 0x4E
    float                   field_0x50;     // 0x50, progress, 0..1
    float                   field_0x54;     // 0x54, the duration the progress is divided by
    u32                     field_0x58;     // 0x58
    unk_TexTransitionNode*  field_0x5C;     // 0x5C, the texture list
    cTexTransition*         field_0x60;     // 0x60, the transition linked before this one
    bool                    field_0x64;     // 0x64, a finished transition is announced once
};

extern u32 lbl_80529700;            // class-name hash of "cTexTransition"
extern unk_801A8F6C lbl_80529704;   // type-registry entry of cTexTransition
extern cTexTransition* lbl_80529708;    // head of the list of live transitions

#endif
