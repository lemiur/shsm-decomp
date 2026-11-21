#ifndef GAME_UNK_800A9064_H
#define GAME_UNK_800A9064_H

#include <revolution/types.h>
#include <game/unk_80024158.h>

/*
 * unk_800A9064: class name invented after its constructor address (no string evidence).
 * Primary base unk_80024158 at 0x0 (constructor 0x80024158, destructor 0x8002474C).
 * Vtable lbl_803E9D4C (size 0x7C): inherited slots 0x8..0x24 come from the base; overrides at
 * 0x10 (destructor 0x800A9110) and 0x14 (0x800A98FC); the other slots are new. Slots 0x6C and
 * 0x74 hold functions of other units (0x80053668, 0x800AB100). Signatures are not yet confirmed:
 * every member takes no arguments until the callers show them.
 */
class unk_800A9064 : public unk_80024158 {
public:
    unk_800A9064();
    virtual ~unk_800A9064();

    virtual void    fn_80024D60();   // overrides unk_80024158::fn_80024D60 (slot 0x14)
    virtual void    fn_800A918C();   // slot 0x28
    virtual void    fn_800A93DC();   // slot 0x2C
    virtual void    fn_800A9BE8();   // slot 0x30
    virtual void    fn_800A97B8();   // slot 0x34
    virtual void    fn_800A99B4( bool flag );   // slot 0x38
    virtual void    fn_800A9840();   // slot 0x3C
    virtual void    fn_800A9DC4();   // slot 0x40
    virtual void    fn_800A9BE4();   // slot 0x44
    virtual void    fn_800A9188();   // slot 0x48
    virtual void    fn_800A9DF0();   // slot 0x4C
    virtual void    fn_800A9DC0();   // slot 0x50
    virtual void    fn_800A983C();   // slot 0x54
    virtual void    fn_800A9B78();   // slot 0x58
    virtual void    fn_800A9B28();   // slot 0x5C
    virtual void    fn_800A9AC8();   // slot 0x60
    virtual void    fn_800A9AD0();   // slot 0x64
    virtual void    fn_800A9C44();   // slot 0x68
    virtual void    fn_80053668();   // slot 0x6C, defined in another unit
    virtual void    fn_800A9C10();   // slot 0x70
    virtual void    fn_800AB100();   // slot 0x74, defined in another unit
    virtual void    fn_800AA0A0();   // slot 0x78

    // fields after the base object (offsets from the classes scan; names not yet evidenced)
    u8              field_0x64[0x3C];   // block copied by fn_800A97B8 (memcpy of 0x30 bytes at 0x64)
    u8              field_0xA0[0x8];
};

#endif
