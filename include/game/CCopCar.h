#ifndef GAME_CCOPCAR_H
#define GAME_CCOPCAR_H

// unit name unconfirmed: the class name is the registration string "CCopCar"; the file name is
// invented.

#include <climax/Memory.h>
#include <game/CGenericCar.h>

/*
 * The police car (registered as "CCopCar", vtable lbl_803E6A10, size 0xA030): a CGenericCar
 * whose five overrides only pass on to the car's own. Member names invented.
 */
class CCopCar : public CGenericCar {
public:
                    CCopCar( const unk_AttributePacket& packet ) : CGenericCar( packet ) {}
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CCopCar();  // override
    virtual void    fn_80019F88( float dt );  // override
    virtual void    fn_800180E4();  // override

    static CAttributeHandler* fn_80077B8C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CCopCar( packet );
    }
};

extern u32 lbl_80529598;            // class-name hash of "CCopCar"
extern unk_801A8F6C lbl_8052959C;   // type-registry entry of CCopCar

#endif
