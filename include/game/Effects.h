#ifndef GAME_EFFECTS_H
#define GAME_EFFECTS_H

#include <revolution/types.h>
#include <rwsdk/plcore/bamatrix.h>
#include <game/EffectBank.h>

struct unk_Msg;     // the engine message (climax/EventHandler.h); name invented

/*
 * TODO: class Effects: base class, members and vtable are not recovered yet. Declared from
 * the mangled names of the unit's functions (return types of unmatched members are unknown and
 * shown as void).
 */
class Effects {
public:
    float CalculateEase( float, float, float );
    static void GetBaseMatrix( RwMatrixTag&, void* );
    static void GetMatrixByTag( unsigned int, RwMatrixTag&, void* );
};

extern "C" {
void fn_80089F94();
void fn_8008A00C();
void fn_8008A188( void*, struct unk_Msg& );
void fn_8008A1C0();
void fn_8008A288();
void fn_8008A378();
void fn_8008A450();
void fn_8008A934();
void fn_8008AC70();
void fn_8008AFB0();
void fn_8008B050();
void fn_8008B0AC();
void fn_8008B31C();
void fn_8008B348();
void fn_8008B9CC( void*, struct unk_EffectNode*, void* );
void fn_8008BA3C();
void fn_8008BC70();
void fn_8008BFC8();
}

#endif
