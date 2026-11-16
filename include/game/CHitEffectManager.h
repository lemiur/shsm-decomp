#ifndef GAME_CHITEFFECTMANAGER_H
#define GAME_CHITEFFECTMANAGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/CSystemCommands.h>
#include <string.h>

/* A loaded effect looked up by name. Name invented; members unknown. */
struct unk_Effect {
    char        field_0x0[0x40];
    u32         field_0x40;
    char        field_0x44[0x61C];
    int         field_0x660;
    const char* field_0x664;        // effect kind, one of the HIT_ kind names
};

/* The owner of the loaded effects. Name invented; members unknown. */
class unk_EffectBank {
public:
    unk_Effect* fn_8008AF18( const char* name );   // defined in 0x8008AF18, unit not yet reconstructed
};

extern unk_EffectBank* lbl_805296E0;

/* The hit and block effects, in CHitEffectManager::effects order. Names invented. */
enum {
    HIT_GENERIC_HIT,
    HIT_GENERIC_BLOCK,
    HIT_PLAYER_HIT,
    HIT_PLAYER_BLOCK,
    NUM_HIT_EFFECTS
};

/*
 * Keeps the effect played for each kind of hit, set from attribute commands that name effects.
 * The single instance is lbl_80529690.
 */
class CHitEffectManager : public CBaseBehaviour {
public:
    /* Attribute commands. Name invented. */
    enum {
        CMD_SetEffect
    };

                    CHitEffectManager();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~CHitEffectManager();

    // clears the effects; name invented
    void            Reset() {
                        for ( int i = 0; i < NUM_HIT_EFFECTS; i++ ) {
                            memset( &effects[i], 0, sizeof( effects[i] ) );
                            effects[i] = 0;
                        }
                    }

    static CAttributeHandler* fn_80087820( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CHitEffectManager;
    }

    u32     effects[NUM_HIT_EFFECTS];   // 0x34, field_0x40 of the effect set for each kind
};

extern CHitEffectManager* lbl_80529690;
extern u32 lbl_80529688;
extern unk_801A8F6C lbl_8052968C;

#endif
