#ifndef CLIMAX_UNK_8007C9E0_H
#define CLIMAX_UNK_8007C9E0_H

#include <revolution/types.h>
#include <game/CBaseBehaviour.h>
#include <climax/unk_801A92A8.h>
#include <climax/Frame.h>
#include <game/CZoneChangeInfo.h>
#include <game/unk_80491B38.h>

/*
 * The registered class "cReflector" (registry string in fn_8007EF58): a CBaseBehaviour whose
 * handlers apply the frame's matrix to the reflected objects. The layout is evidenced by the
 * accesses of the constructor and the handlers; fields with no known role are named by offset.
 */
class cReflector : public CBaseBehaviour {
public:
    virtual void    fn_801A7A88();
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    u8              field_0x34[0x1C];   // 0x34, cReflector's own vptr (its new slots are not recovered yet), then undetermined
    unk_RpAtomic*   field_0x50;         // 0x50, the atomic the zone changes add to and remove from
    u8              field_0x54[0x14];   // 0x54, undetermined
    unk_RwMatrix    field_0x68;         // 0x68, the matrix the reflection multiplies into
    bool            field_0xA8;         // 0xA8, set by the message at field_0xAC, cleared by field_0xB4
    u8              field_0xA9[0x3];    // 0xA9, undetermined
    unk_801A92A8    field_0xAC;         // 0xAC, message id
    unk_801A92A8    field_0xB4;         // 0xB4, message id
    unk_801A92A8    field_0xBC;         // 0xBC, message id
    CZone*          field_0xC4;         // 0xC4, the zone last attached by a zone change
};

extern unk_RwMatrix lbl_80469F30;    // 0x80469F30, the matrix the texture matrices are built from
void RwClimaxRVLConvertMatrix( const unk_RwMatrix* in, f32 out[3][4] );

// Message ids handled by cReflector; defined elsewhere, unit not yet reconstructed.
extern unk_801A92A8 lbl_8052A858;
extern unk_801A92A8 lbl_8052A860;

extern "C" {
void fn_8007C9E0();
void fn_8007CE84();
void fn_800276E8( CZone* zone, unk_RpAtomic* atomic );
int fn_8007D4C0( unk_RwMatrix* matrix );
void fn_8007D52C();
void fn_8007D698();
void fn_8007D76C( CBaseBehaviour* behaviour, unk_80491B38* player );
void fn_8007E56C();
void fn_8007E694();
void fn_8007E7D4();
void fn_8007EC0C();
void fn_8007ED6C();
void fn_8007EE60();
void fn_8007EEDC();
void fn_8007EFE4();
void fn_8007F02C();
void fn_8007F034();
void fn_8007F03C();
}

#endif
