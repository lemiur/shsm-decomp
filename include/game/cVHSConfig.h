#ifndef GAME_CVHSCONFIG_H
#define GAME_CVHSCONFIG_H

#include <climax/AttributeHandler.h>
#include <climax/EventHandler.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

class cVHSConfig;

extern u32 lbl_80529718;                // class-name hash of "cVHSConfig"
extern unk_801A8F6C lbl_8052971C;       // type-registry entry of cVHSConfig
extern cVHSConfig* lbl_80529720;        // the live configs, newest first, linked through next

/*
 * A value set by an attribute that is read while the config is active. The on message activates
 * the config and the off message deactivates it; fn_800911F0 reads the largest value of the active
 * configs. Each new config is pushed on lbl_80529720 and taken off it when it is destroyed.
 * vtable lbl_803E87E0, size 0x50. Member names invented.
 */
class cVHSConfig : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetValue,       // float: the value
        CMD_SetActive,      // integer: nonzero sets active
        CMD_SetOnMsg,       // text: the name of the message that activates the config
        CMD_SetOffMsg       // text: the name of the message that deactivates the config
    };

                    cVHSConfig() {
                        active = false;
                        onEvent.fn_801A92FC( NULL );
                        offEvent.fn_801A92FC( NULL );
                        next = lbl_80529720;
                        lbl_80529720 = this;
                    }
    virtual         ~cVHSConfig();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800913F8( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) cVHSConfig();
    }

    cVHSConfig*     next;           // 0x34, the next live config
    bool            active;         // 0x38
    float           value;          // 0x3C
    unk_801A92A8    onEvent;        // 0x40
    unk_801A92A8    offEvent;       // 0x48
};

/* Returns the largest value of the active configs, or 0.0 when none is active. Name invented. */
float fn_800911F0();

#endif
