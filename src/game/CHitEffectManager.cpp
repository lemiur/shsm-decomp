// CHitEffectManager: the class-name string "CHitEffectManager" is registered here.
// The inline factory fn_80087820 and the two thunks follow the static initialiser in .text.

#include <game/CHitEffectManager.h>

u32 lbl_80529688 = fn_801A7A84( "CHitEffectManager" );
unk_801A8F6C lbl_8052968C( "CHitEffectManager", CHitEffectManager::fn_80087820, sizeof( CHitEffectManager ) );

CHitEffectManager* lbl_80529690;

/*
====================
CHitEffectManager::CHitEffectManager

Becomes the instance and clears the effects. Expanded in fn_80087820.
====================
*/
inline CHitEffectManager::CHitEffectManager() {
    lbl_80529690 = this;
    lbl_80529690->Reset();
}

/*
====================
CHitEffectManager::~CHitEffectManager
====================
*/
// 0x80087614
CHitEffectManager::~CHitEffectManager() {
    if ( lbl_80529690 == this ) {
        lbl_80529690 = NULL;
    }
}

/*
====================
CHitEffectManager::fn_8009CB1C
====================
*/
// 0x80087684
void CHitEffectManager::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
}

/*
====================
CHitEffectManager::fn_801A73A8

Takes the effect for each kind of hit from the effects the packet names.
====================
*/
// 0x80087688
void CHitEffectManager::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_805297E8 );
    while ( !it.IsFinished() ) {
        if ( it->id == CMD_SetEffect ) {
            unk_Effect* effect = lbl_805296E0->fn_8008AF18( it->GetString() );
            if ( effect != NULL && effect->field_0x660 ) {
                const char* kindName = effect->field_0x664;
                int kind = NUM_HIT_EFFECTS;
                if ( strcmp( kindName, "Generic_Hit" ) == 0 ) {
                    kind = HIT_GENERIC_HIT;
                } else if ( strcmp( kindName, "Generic_Block" ) == 0 ) {
                    kind = HIT_GENERIC_BLOCK;
                } else if ( strcmp( kindName, "Player_Hit" ) == 0 ) {
                    kind = HIT_PLAYER_HIT;
                } else if ( strcmp( kindName, "Player_Block" ) == 0 ) {
                    kind = HIT_PLAYER_BLOCK;
                }
                if ( kind != NUM_HIT_EFFECTS ) {
                    effects[kind] = effect->field_0x40;
                }
            }
        }
        ++it;
    }
}
