#ifndef GAME_CCRAYON_H
#define GAME_CCRAYON_H

// unit name unconfirmed: the class name is the registration string "CCrayon"; its functions and
// registration are in the unit of CDrawingTest (game/CDrawingTest.cpp).

#include <climax/Memory.h>
#include <game/CBasicAnimatedObject.h>

/*
 * A crayon of the painting test (registered as "CCrayon", vtable lbl_803FBB10, size 0x1E0): an
 * animated object with a colour and a size that files itself in the crayon table lbl_804780C0
 * under its index. Member names invented.
 */
class CCrayon : public CBasicAnimatedObject {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetIndex,   // integer: the slot in lbl_804780C0
        CMD_SetColour,  // colour: red, green and blue (the alpha is always 255)
        CMD_SetSize     // float
    };

                    CCrayon( const unk_AttributePacket& packet ) : CBasicAnimatedObject( packet ) {
                        size = 6.0f;
                        index = 0;
                    }
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CCrayon();  // override
    virtual void    fn_801A7A88();  // override

    static CAttributeHandler* fn_8017A2B8( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CCrayon( packet );
    }

    u32             index;      // 0x1D4, the slot in lbl_804780C0
    unk_RGBA        colour;     // 0x1D8
    float           size;       // 0x1DC
};

extern u32 lbl_8052A344;            // class-name hash of "CCrayon"
extern unk_801A8F6C lbl_8052A348;   // type-registry entry of CCrayon
extern CCrayon* lbl_804780C0[12];   // the crayons by index; defined in game/CDrawingTest.cpp

#endif
