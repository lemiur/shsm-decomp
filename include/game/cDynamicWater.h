#ifndef GAME_CDYNAMICWATER_H
#define GAME_CDYNAMICWATER_H

#include <revolution/types.h>
#include <climax/Atomic.h>
#include <game/CBaseBehaviour.h>
#include <game/CZoneChangeInfo.h>
#include <game/unk_800276E0.h>
#include <game/unk_80491B38.h>

// Defined in 0x800276E8, in the CZone unit (not yet reconstructed): removes atomic from the world
// it is in (RpAtomicGetWorld, then RpWorldRemoveAtomic) and returns that world. The zone is not read.
extern "C" void fn_800276E8( CZone* zone, unk_RpAtomic* atomic );

/*
 * The water behaviour named "cDynamicWater" by the class registry in fn_80084BC0 (factory
 * 0x80084D0C, object size 0x1B0, vtable lbl_803E7C60). Only the members below are reconstructed so far.
 */
class cDynamicWater : public CBaseBehaviour {
public:
    // 0x80081F6C, overrides CAttributeHandler::fn_801A7BFC (the zone-change slot)
    // 0x80081E00, overrides CBaseBehaviour::~CBaseBehaviour; releases the members and the textures
    virtual         ~cDynamicWater();
    virtual void    fn_801A7BFC( CZoneChangeInfo* change );  // override
    // 0x80081FDC, overrides CBaseBehaviour::fn_8009CB1C (the message handler)
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override

    u8              field_0x34[0x8];    // not yet reconstructed
    RwTexture*      field_0x3c;         // a texture the destructor destroys
    u8              field_0x40[0x8];    // not yet reconstructed
    RwTexture*      field_0x48;         // a texture the destructor destroys
    u8              field_0x4c[0x8];    // not yet reconstructed
    RwTexture*      field_0x54;         // a texture the destructor destroys
    u8              field_0x58[0x8];    // not yet reconstructed
    RwTexture*      field_0x60;         // a texture the destructor destroys
    u8              field_0x64[0x10];   // not yet reconstructed
    unk_RpAtomic*   field_0x74;         // the water's atomic: added to the world of the zone entered, removed from the one left
    unk_RwMatrix    field_0x78;         // the inverse of the frame's modelling matrix (message field_0x19c)
    unk_RwMatrix    field_0xb8;         // the product of the message matrix and field_0x78 (message field_0x1a4)
    unk_RwMatrix    field_0xf8;         // multiplied with the message matrix into the frame's modelling matrix (message field_0x19c)
    unk_RwMatrix    field_0x138;        // a copy of the message matrix (message field_0x1a4)
    u8              field_0x178[0x14];  // not yet reconstructed
    unk_801A92A8    field_0x18c;        // message id, released by the destructor (names not evidenced)
    unk_801A92A8    field_0x194;        // message id, released by the destructor (names not evidenced)
    unk_801A92A8    field_0x19c;        // message id, linked in the constructor (names not evidenced)
    unk_801A92A8    field_0x1a4;        // message id, linked in the constructor (names not evidenced)
    u8*             field_0x1ac;        // an array allocated with new[] and released by the destructor (element type not evidenced)
};

// Message id, registered by the constructor and defined in its owning unit (not yet reconstructed).
extern unk_801A92A8 lbl_8052A788;
// The static the destructor clears when it is the object it holds (defined in this unit, not yet reconstructed).
extern cDynamicWater* lbl_80529640;

extern "C" {
void fn_80081928();
void fn_80082110();
void fn_80082488();
void fn_80082588();
void fn_8008288C();
void fn_80082B70();
void fn_80082D3C( cDynamicWater* self, const void* hit );
void fn_80083250();
void fn_80083318();
void fn_800833D8();
void fn_800834E8();
void fn_80083580();
void fn_80083688();
void fn_800836D8();
void fn_8008393C();
void fn_80083A60();
void fn_80083BD4();
void fn_80083C38();
void fn_80083EC0();
void fn_80084060();
void fn_8008413C();
void fn_8008423C();
void fn_80084404();
void fn_80084678();
void fn_80084828();
void fn_800848F0();
void fn_80084A5C();
void fn_80084D0C();
void fn_80084D54();
void fn_80084D9C();
void fn_80084DE4();
void fn_80084E98();
void fn_80084F38();
void fn_80084F40();
void fn_80084F48();
void fn_80084F50();
void fn_80084F58();
void fn_80084F60();
void fn_80084F68();
void fn_80084F70();
void fn_80084F78();
void fn_80084F80();
void fn_80084F88();
}

#endif
