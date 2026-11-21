// Unit name and boundary: class Sfx::EffectWrapper (dtk signature name BaseMatrix__Q23Sfx13EffectWrapperFR11RwMatrixTagPv); file name from the Gormiti link map (Effects/EffectWrapper.cpp, EffectWrapper.obj)

#include <game/EffectWrapper.h>

/*
====================
fn_80084F90
====================
*/
// 0x80084F90
// Clears the instance handle, then invokes the callback with the wrapper and its callback data. Name is a placeholder:
// the sibling's EffectInstanceDestroy has the same signature, but its body differs from this one.
extern "C" void fn_80084F90( Effects::SFX_instance* instance, void* data ) {
    Sfx::EffectWrapper* This = (Sfx::EffectWrapper*)data;
    void ( *callback )( void*, void* ) = This->m_callbackFn;

    This->m_instanceHandle = NULL;
    if ( callback != NULL ) {
        callback( This, This->m_callbackData );
    }
}

/*
====================
Sfx::EffectWrapper::BaseMatrix
====================
*/
// 0x80084FB8
// Copies the wrapper's matrix into the caller's matrix. The second argument is the wrapper itself.
void Sfx::EffectWrapper::BaseMatrix( RwMatrixTag& matrix, void* pData ) {
    Sfx::EffectWrapper* This = (Sfx::EffectWrapper*)pData;

    matrix = This->m_matrix;
}
