#ifndef GAME_CICE_H
#define GAME_CICE_H

// cIce: the class name is the registry string "cIce" in fn_80087D88, and the factory fn_80087E0C
// builds it (size 0x148). Its vtable lbl_803E80D8 is in this unit's .data. The base unk_8009D2E4
// is the scene behaviour (vtable lbl_803E8D78), reconstructed in its own header.

#include <game/unk_8009D2E4.h>
#include <climax/Atomic.h>
#include <climax/Clump.h>
#include <revolution/mtx.h>
#include <revolution/gx/GXGet.h>
#include <revolution/gx/GXTransform.h>
#include <revolution/gx/GXEnum.h>
#include <game/CZoneChangeInfo.h>

class cIce : public unk_8009D2E4 {
public:
    cIce( const unk_AttributePacket& packet ) : unk_8009D2E4( packet ) {
        field_0x144 = NULL;
    }

    virtual void    fn_80087D78( unk_Msg& msg );                        // override of fn_8009CB1C
    virtual void    fn_80087D7C( const unk_AttributePacket& packet );   // override of fn_801A73A8
    virtual         ~cIce();                                             // override
    virtual void    fn_80087974( CZoneChangeInfo* change );             // override of fn_801A7BFC
    virtual void    fn_80087D14();                                       // override of CAttributeHandler::fn_801A7A88

    /* The registry factory: the instance comes from the game heap, its constructor inline.
       Open: the bytes match, but the vptr at 0x2C points its slot 0x18 (the override fn_80087D14 of
       CAttributeHandler::fn_801A7A88 in the secondary base) at the base slot, so two relocation
       targets differ (the same layout gap as the destructor). */
    static CAttributeHandler* fn_80087E0C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) cIce( packet );
    }

    unk_RpClump*    field_0x144;    // 0x144: the ice's clump, destroyed with the object
};

extern "C" {

// RenderWare 3.7 layer calls used by the atomic callbacks; declared here because the SDK headers
// clash with climax/Frame.h (rwCOMBINEREPLACE).
void            RwClimaxAtomicSetPass( unk_RpAtomic* atomic, u32 pass );
unk_RwFrame*    RwFrameRemoveChild( unk_RwFrame* child );

// Atomic render callbacks (renderCallBack at 0x48 of the atomic) and the clump callbacks.
unk_RpAtomic*   fn_80087BB4( unk_RpAtomic* atomic );
unk_RpAtomic*   fn_80087BE4( unk_RpAtomic* atomic );
unk_RpAtomic*   fn_80087B2C( unk_RpAtomic* atomic );
void            fn_80087CD0( unk_RpAtomic* atomic );
unk_RpAtomic*   fn_80087CF4( unk_RpAtomic* atomic, void* data );
unk_RpAtomic*   fn_80087C74( unk_RpAtomic* atomic, void* data );
void            fn_80087D80( u8 flag );
void            fn_800879FC();
unsigned int    fn_80087AC0( unk_RwMatrix* ltm, int normals, float ( *ModelView )[4] );

// The texture-matrix target (lbl_8046A510), the inverse camera matrix of the display list
// (_RwDlInvCamLTM, climax/Frame.h) and the GX-to-RVL matrix conversion: globals and calls of units not yet
// reconstructed.
extern unk_RwMatrix lbl_8046A510;
void            RwClimaxRVLConvertMatrix( const unk_RwMatrix* rwMatrix, Mtx mtx );

// defined in 0x800BE62C, unit not yet reconstructed (the atomic pass setup called by fn_80087C74)
void            fn_800BE62C( unk_RpAtomic* atomic );

// The zone's world: the CZone member at 0xA4 (identity TODO, CZone unit not reconstructed); retail reads it inline.
struct RpWorld;
RpWorld*        CZoneGetWorld( CZone* zone );

// RenderWare world calls; the SDK header (rwsdk/world/baworobj.h) is not included here, because its
// enums clash with climax/Frame.h.
RpWorld*        RpWorldAddClump( RpWorld* world, unk_RpClump* clump );
RpWorld*        RpWorldRemoveClump( RpWorld* world, unk_RpClump* clump );

// Set from the render code (fn_80087D80); a flag byte in this unit's .sbss.
extern u8       lbl_8052969D;
}

#endif
