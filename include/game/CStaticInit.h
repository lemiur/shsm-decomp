#ifndef GAME_CSTATICINIT_H
#define GAME_CSTATICINIT_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * Sends its output message once, on the first frame message lbl_8052A788 after it is made, then
 * stops listening to the frame. vtable __vt__11CStaticInit, size 0x3C. Member names invented.
 */
class CStaticInit : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetOutputMsg    // text: the name of the message sent on the first frame
    };

                    CStaticInit();
    virtual         ~CStaticInit();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800D0354( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CStaticInit;
    }

    unk_801A92A8    outputMsg;  // 0x34
};

extern u32 lbl_80529BE0;            // class-name hash of "CStaticInit"
extern unk_801A8F6C lbl_80529BE4;   // type-registry entry of CStaticInit

#endif
