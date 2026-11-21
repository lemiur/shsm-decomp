#ifndef GAME_UNK_80031F54_H
#define GAME_UNK_80031F54_H

// unit name unconfirmed: the three functions below are the render pass of the object they
// receive; the owning class is not yet reconstructed (names invented).

#include <revolution/types.h>
#include <rwsdk/world/climaxplatform.h>
#include <rwsdk/world/climaxcommon.h>
#include <climax/RenderState.h>
// fn_80009628 is declared in game/unk_80009628.h; that header does not compile on its own here.

// Placeholder: the render object the pass runs on. Name invented; only the members the pass
// uses are named, the rest of the layout is not yet reconstructed.
class unk_RenderObject {
public:
    u8 field_0x0[0xA4];
    RpWorld* field_0xA4;        // the world RwClimaxWorldRender draws
    u8 field_0xA8[0x150 - 0xA8];
    u8 field_0x150;             // when set, the alpha pass runs
};

// Placeholder: an entry of the list at lbl_80529638. Name invented.
struct unk_RenderNode {
    u8 field_0x0[0x34];
    unk_RenderNode* field_0x34; // next entry
    u8 field_0x38[0x54 - 0x38];
    u8 field_0x54;
    u8 field_0x55[0x69 - 0x55];
    u8 field_0x69;
};

// Declared here only: the unit that owns lbl_80529638 is not yet identified (it is shared with fn_80083318).
extern unk_RenderNode* lbl_80529638;

// Callees in other units: names and signatures not yet reconstructed; object arguments as void*.
extern "C" {
void fn_800285BC( void* self, u32 mode );
void fn_800C07B4( void* self, u32 mode );
void fn_8008E120( void* self, u32 mode );
void fn_80083BD4( void* self );
void fn_80009628( void* owner );
void fn_8007F20C( void* arg, u32 mode );
void fn_800927BC( void* arg );
void fn_80097564( void* node );
void fn_801D9DD0( u32 mode );
void ClimaxT1ResetAlphaPass( void );
void SetFogOff__11RenderStateFv( void* self );
void SetRefractionDirty__6CBloomFv( void );
}

extern "C" {
void fn_80031F54( unk_RenderObject* self, void* arg );
void fn_80032240( unk_RenderObject* self, void* arg );
void fn_80032244( unk_RenderObject* self, void* arg );
}

#endif
