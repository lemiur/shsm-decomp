#ifndef GAME_EFFECTWRAPPER_H
#define GAME_EFFECTWRAPPER_H

#include <revolution/types.h>
#include <rwsdk/plcore/bamatrix.h>

namespace Effects {
class SFX_type;
class SFX_instance;
}

namespace Characters {
class CCharacterBehaviour;
}

namespace Sfx {

/*
 * The game-side owner of one special effect instance. The layout follows the sibling's DWARF for
 * Sfx::EffectWrapper (size 0x58); the offsets this unit's code uses (0x04, 0x08, 0x48, 0x4C) are confirmed by retail.
 */
class EffectWrapper {
public:
    static void BaseMatrix( RwMatrixTag& matrix, void* pData );

    Effects::SFX_type* m_typeHandle;                 // 0x00
    Effects::SFX_instance* m_instanceHandle;         // 0x04
    RwMatrixTag m_matrix;                            // 0x08
    void ( *m_callbackFn )( void*, void* );          // 0x48, retail calls it with two arguments
    void* m_callbackData;                            // 0x4C
    Characters::CCharacterBehaviour* m_pCharacter;   // 0x50
    bool m_visible;                                  // 0x54
};

}

// Placeholder name: the SFX instance destroy callback (two arguments, instance and data).
extern "C" void fn_80084F90( Effects::SFX_instance* instance, void* data );

#endif
