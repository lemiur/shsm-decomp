#ifndef GAME_CMAPLAYER_H
#define GAME_CMAPLAYER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Map.h>

/*
 * Hands a map layer's name and a flag to the map when its message arrives; the message, the
 * name and the flag are set by the attribute packet. vtable __vt__9CMapLayer, size 0x44.
 * Member names invented.
 */
class CMapLayer : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetMsg,     // text: the name of the message that triggers it
        CMD_SetName,    // text: the name of the layer
        CMD_SetFlag     // integer: the flag is set when it is not zero
    };

                    CMapLayer() {
                        name = NULL;
                        flag = true;
                    }
    virtual         ~CMapLayer();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_8018ADA0( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CMapLayer;
    }

    unk_801A92A8    triggerMsg; // 0x34
    const char*     name;       // 0x3C, points into the attribute packet
    bool            flag;       // 0x40
};

extern u32 lbl_8052A3E0;            // class-name hash of "CMapLayer"
extern unk_801A8F6C lbl_8052A3E4;   // type-registry entry of CMapLayer

#endif
