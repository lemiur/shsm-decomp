#ifndef GAME_UNK_8015F670_H
#define GAME_UNK_8015F670_H

#include <game/unk_8009D2E4.h>
#include <game/PadRumble.h>

/*
 * The one-slot polymorphic interface at 0x144 of unk_8015F670 (its vtable group is a header and
 * one slot; no standalone vtable, constructor or destructor is known). Name invented; the slot is
 * named after unk_8015F670's implementation.
 */
class unk_8015F670Base144 {
public:
    virtual void    fn_80160F08();
};

/* What unk_8015F670's field_0x1A8 points to: only the float at 0xC is read. Name invented. */
struct unk_8015F670Rate {
    u8              field_0x0[0xC];
    float           field_0xC;      // scales unk_8015F670::fn_80160584's result
};

/*
 * The base of unk_80161958, the puzzle behaviours (vtable lbl_803F9470, size 0x120, constructor
 * 0x8015F670, destructor 0x8015F858; unit not yet reconstructed): unk_8009D2E4, then the interface
 * unk_8015F670Base144 at 0x144. fn_8000CFD0, fn_8000CFCC and fn_80160F08 override slots of
 * CAttributeHandler and of the interface, so they take new entries at 0xF8, 0xFC and 0x11C. The
 * object is 0x23C bytes; its own data is padding here except the fields this code reads. Slot
 * signatures are known only where recovered code calls them.
 */
class unk_8015F670 : public unk_8009D2E4, public unk_8015F670Base144 {
public:
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_8015F670();  // override
    virtual void    fn_8000CFD0();  // override
    virtual void    fn_8000CFCC();  // override
    virtual void    fn_80160814();
    virtual void    fn_801607D0();
    virtual void    fn_80160614( float value );
    virtual void    fn_801606BC( float value );
    virtual void    fn_80160748( float value );
    virtual float   fn_80160584( float t );
    virtual void    fn_8015FFB8();
    virtual void    fn_80160F08();  // override

    // defined in 0x8015F62C, 0x80160DDC, 0x80160E10 and 0x80160ECC, unit not yet reconstructed
    bool            fn_8015F62C();
    bool            fn_80160DDC();
    bool            fn_80160E10( const unk_Tweaks* tweaks );
    bool            fn_80160ECC( const unk_Tweaks* tweaks );

    u8              field_0x148[0x20];
    u8              field_0x168;        // flags
    u8              field_0x169[0x3F];
    unk_8015F670Rate* field_0x1A8;      // NULL: the rate comes from the unk_80024158 base
    u8              field_0x1AC[0x90];
};

#endif
