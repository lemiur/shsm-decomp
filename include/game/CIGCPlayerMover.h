#ifndef GAME_CIGCPLAYERMOVER_H
#define GAME_CIGCPLAYERMOVER_H

#include <climax/Frame.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

extern unk_801A92A8 lbl_80529860;           // the RespotPlayer message (payload RwMatrix*)
extern u32 lbl_80529868;                    // class-name hash of "CIGCPlayerMover"
extern unk_801A8F6C lbl_8052986C;           // type-registry entry of CIGCPlayerMover
extern const char* const lbl_8052C3F0;      // "RespotPlayer"

/*
 * Respots the player at its own frame: posts RespotPlayer with the frame's modelling matrix
 * when its message arrives; the message is set by the attribute packet.
 * vtable __vt__15CIGCPlayerMover, size 0x3C. Member names invented.
 */
class CIGCPlayerMover : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTriggerMsg   // text: the name of the message that respots the player
    };

                    CIGCPlayerMover() {
                        fn_801A93B4( lbl_80529860, lbl_8052C3F0, "RwMatrix*" );
                    }
    virtual         ~CIGCPlayerMover();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800AB4B4( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CIGCPlayerMover;
    }

    unk_801A92A8    triggerMsg;     // 0x34
};

#endif
