#ifndef GAME_CBASICMOUNT_H
#define GAME_CBASICMOUNT_H

#include <climax/Frame.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

/*
 * A mount point as the mount message carries it (the payload type string "s_Mount*"): the
 * mount's matrix, an offset matrix and the behaviour that owns it. Size 0x98. Member names
 * invented.
 */
struct s_Mount {
    unk_RwMatrix    matrix;         // 0x0, the owner's frame matrix when the message is sent
    unk_RwMatrix    offset;         // 0x40, the identity
    s32             field_0x80;     // 0x80
    void*           owner;          // 0x84, the CBasicMount
    u32             field_0x88;     // 0x88
    bool            field_0x8C;     // 0x8C
    u32             field_0x90;     // 0x90
    u32             field_0x94;     // 0x94
};

/*
 * Sends its mount point each frame with the message named by the attribute packet.
 * vtable __vt__11CBasicMount, size 0xD4. Member names invented.
 */
class CBasicMount : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetMountMsg     // text: the name of the message that carries the mount point
    };

                    CBasicMount( const unk_AttributePacket& packet );
    virtual         ~CBasicMount();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_8009D28C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CBasicMount( packet );
    }

    unk_801A92A8    mountMsg;       // 0x34
    s_Mount         mount;          // 0x3C
};

extern u32 lbl_805297B0;            // class-name hash of "CBasicMount"
extern unk_801A8F6C lbl_805297B4;   // type-registry entry of CBasicMount

#endif
