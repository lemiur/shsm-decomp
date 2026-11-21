#ifndef GAME_CSMASHABLEOBJECT_H
#define GAME_CSMASHABLEOBJECT_H

#include <climax/EventHandler.h>
#include <game/unk_8009D2E4.h>

struct unk_RwpBody;

/*
 * A scene behaviour that counts hits against a limit read from its attribute packet and posts
 * its event id (field_0x230) when the count reaches it. Registered as "CSmashableObject" (DOL
 * string). Constructor 0x800A72E4, destructor 0x800A7380, attribute handler 0x800A742C. Member
 * names invented.
 */
class CSmashableObject : public unk_8009D2E4 {
public:
                    CSmashableObject( const unk_AttributePacket& packet );
    virtual         ~CSmashableObject();  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual void    fn_8000CFD0();  // override
    virtual void    fn_8000CFCC();  // override
    virtual void    fn_8009E6D4();  // override
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_800A76CC();
    virtual void    fn_8000F5DC();  // override of unk_803E8E64
    virtual void    fn_8000F5E0();  // override of unk_803E8E64
    virtual void    fn_800A7BC4( unk_Vec4* out );
    virtual void    fn_80017558();  // override of unk_80024158
    virtual void    fn_80017554();  // override of unk_80024158
    virtual void    fn_8001755C();  // override of unk_80024158

    void            fn_800A7704();
    void            fn_800A7AF4();

    s32             field_0x144;    // the hit limit: attribute 0 of the packet
    s32             field_0x148;    // the hits counted so far
    f32             field_0x14C;
    u8              field_0x150[0x4];
    f32             field_0x154;
    s32             field_0x158;
    s32             field_0x15C;
    unk_RwpBody*    field_0x160;    // physics body, released by fn_800A7AF4
    unk_RwpBody*    field_0x164;    // physics body, released by fn_800A7AF4
    u8              field_0x168[0x4C];
    s32             field_0x1B4;
    s32             field_0x1B8;
    unk_RwpBody*    field_0x1BC;    // physics body, released by fn_800A7AF4
    unk_RwpBody*    field_0x1C0;    // physics body, released by fn_800A7AF4
    u8              field_0x1C4[0x40];
    f32             field_0x204;
    f32             field_0x208;
    f32             field_0x20C;
    u8              field_0x210[0x18];
    s32             field_0x228;    // nonzero while the object holds its event
    s32             field_0x22C;
    unk_801A92A8    field_0x230;    // the event id of this object
};

#endif
