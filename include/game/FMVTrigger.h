#ifndef GAME_FMVTRIGGER_H
#define GAME_FMVTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/unk_RespotManager.h>

/*
 * Hands the name of a film to the manager behind lbl_80529260 when its message arrives; the
 * message and the film are named by the attribute packet. vtable __vt__10FMVTrigger, size 0x40.
 * Member names invented.
 */
class FMVTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg,  // text: the name of the message that starts the film
        CMD_SetFilm         // text: the name of the film
    };

                    FMVTrigger() : film( NULL ) {}
    virtual         ~FMVTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800CA280( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) FMVTrigger;
    }

    unk_801A92A8    triggerMsg; // 0x34
    char*           film;       // 0x3C, owned copy of the film name
};

extern u32 lbl_80529B18;            // class-name hash of "FMVTrigger"
extern unk_801A8F6C lbl_80529B1C;   // type-registry entry of FMVTrigger

#endif
