#ifndef GAME_CSKYDOME_H
#define GAME_CSKYDOME_H

#include <climax/Memory.h>
#include <game/unk_8009D2E4.h>
#include <game/unk_ClumpBounds.h>

/*
 * The sky dome behaviour: registered as "CSkyDome" by the static initialiser at 0x800A720C, vtable
 * lbl_803E9858 (size 0xEC), object size 0x168, constructor 0x800A6A60, destructor 0x800A6B34,
 * factory 0x800A726C. It holds the bounds of its clump at 0x150 and joins the list of live sky
 * domes, linked through `next` at 0x164. Member names invented.
 */
class CSkyDome : public unk_8009D2E4 {
public:
                    CSkyDome( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CSkyDome();  // override

    static CAttributeHandler* fn_800A726C( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSkyDome( packet );
    }

    float               field_0x144;    // written from the packet by fn_801A73A8 (0x800A6C80)
    float               field_0x148;    // likewise
    u8                  field_0x14C;
    unk_ClumpBounds*    bounds;         // 0x150, the bounds of the clump of unk_80024158
    unk_801A92A8        field_0x154;    // event ids, unregistered by the destructor
    unk_801A92A8        field_0x15C;
    CSkyDome*           next;           // 0x164, the next sky dome in the live list
};

#endif
