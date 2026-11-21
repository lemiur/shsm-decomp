#ifndef GAME_CSUV_H
#define GAME_CSUV_H

#include <revolution/types.h>
#include <game/CGenericCar.h>

// unit name unconfirmed: the class name is the registration string "CSUV"; the file name is
// invented. The bases and the members beyond CGenericCar are not reconstructed yet.

/*
 * The sport utility vehicle (registered as "CSUV", vtable lbl_803E75F4, size 0xA070). A
 * CGenericCar whose two float overrides pass on to the car's own. Member names invented.
 */
class CSUV : public CGenericCar {
public:
    virtual void    fn_80019F88( float dt );  // override
    virtual void    fn_8001A044( float dt );  // override
};

extern "C" {
void fn_8007BB78();
void fn_8007BD44();
void fn_8007BF40();
void fn_8007C3B0();
void fn_8007C490();
void fn_8007C52C();
void fn_8007C548();
void fn_8007C8A4();
void fn_8007C990();
void fn_8007C998();
void fn_8007C9A0();
void fn_8007C9A8();
void fn_8007C9B0();
void fn_8007C9B8();
void fn_8007C9C0();
void fn_8007C9C8();
void fn_8007C9D0();
void fn_8007C9D8();
void fn_8007BCE8();
}

#endif
