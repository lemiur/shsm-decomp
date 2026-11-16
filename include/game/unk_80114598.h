#ifndef GAME_UNK_80114598_H
#define GAME_UNK_80114598_H

#include <game/CBaseBehaviour.h>

/*
 * The base of the cameras (vtable lbl_803F5250, size 0xA4, constructor 0x80114598, destructor
 * 0x8011465C; unit not yet reconstructed): CPlayerCamera, CPOVCamera, CSimpleStaticCamera,
 * CStaticCamera, 0x80112C24's class and lbl_803EAA68's class derive from it. Its constructor
 * registers the camera in the camera table of unk_800E671C (fn_801152AC). CBaseBehaviour is its
 * only polymorphic base, so its own slots follow CBaseBehaviour's in the one table its vptr at
 * 0x2C points to (0x58 to 0xA0); the first is pure (a null entry). Name from its constructor
 * address (invented); slot names are the addresses of the base's functions in its vtable (the pure
 * slot takes lbl_803EAA68's override), signatures unknown unless noted. The object is 0x78 bytes
 * (CPlayerCamera's own data starts there).
 */
class unk_80114598 : public CBaseBehaviour {
public:
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override (0x801147AC)
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override (0x801147FC)
    virtual         ~unk_80114598();  // override
    virtual void    fn_800B5CBC() = 0;
    virtual void    fn_800B5B90();
    virtual void    fn_800B5B80();
    virtual void    fn_8011471C();
    virtual void    fn_800B5BE8();
    virtual void    fn_800B5BE0();
    // false here, true for CPlayerCamera (0x8011B7BC): the camera's facing counts as one of the
    // player's view directions for points of interest
    virtual bool    fn_800B5B78();
    // false here, true for CPOVCamera (0x800B5B94): points of interest are weighed from the screen
    // centre instead of the player's aim point
    virtual bool    fn_800B5BD8();
    virtual void    fn_80114A6C();
    virtual void    fn_801147F0();
    virtual void    fn_800B5BD0();
    virtual void    fn_800B5BC8();
    virtual void    fn_800B5BC4();
    virtual void    fn_800B554C();
    virtual void    fn_800B5B74();
    virtual void    fn_800B5B70();
    virtual void    fn_800B5B68();
    virtual void    fn_800B5B64();
    virtual void    fn_80114A18();

    u8              field_0x34[0x44];   // includes unk_801A92A8 member objects at 0x50 and 0x58
};

#endif
