#ifndef GAME_UNK_80024158_H
#define GAME_UNK_80024158_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <climax/Clump.h>

/*
 * The polymorphic base at 0x3C of unk_8009D2E4 (vtable lbl_803E1108, size 0x28, constructor
 * 0x80024158, destructor 0x8002474C; eight slots). Its object runs to 0x5C, then an unk_801A92A8
 * member (shown here as padding). Its first two slots share their names with CBaseBehaviour's
 * message and attribute handlers (0x8002487C, 0x8002490C): the behaviours override both with one
 * function each.
 */
class unk_80024158 {
public:
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~unk_80024158();
    virtual void    fn_80024D60();
    virtual void    fn_80017558();
    virtual void    fn_80017554();
    virtual void    fn_8001755C();
    virtual void    fn_80024F18();

    // defined in 0x80024800 and 0x80024A58, unit not yet reconstructed
    void            fn_80024800();
    float           fn_80024A58();
    // defined in 0x80024C74 and 0x80024A14, unit not yet reconstructed: the first takes a float
    // (0 from CSceneObject); the second returns the clump the object holds (its field at 0x54)
    void            fn_80024C74( float value );
    unk_RpClump*    fn_80024A14();

    u8              field_0x4[0x60];   // the rest of the object, including the member at 0x5C
};

#endif
