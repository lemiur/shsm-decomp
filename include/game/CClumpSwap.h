#ifndef GAME_CCLUMPSWAP_H
#define GAME_CCLUMPSWAP_H

#include <climax/Clump.h>
#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

// defined in 0x800A275C, unit not yet reconstructed: the asset of the named RenderWare type
// ("rwID_CLUMP") that the packet names, or NULL
void* fn_800A275C( const unk_AttributePacket& packet, const char* typeName );

/*
 * Makes a copy of the clump its attribute packet names and, on fn_801A7A88, files it in the
 * clump swap list lbl_805297D0 under its name. vtable __vt__10CClumpSwap, size 0x3C. Member names
 * invented.
 */
class CClumpSwap : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetName     // text: the name the clump is filed under
    };

                    CClumpSwap( const unk_AttributePacket& packet );
    virtual         ~CClumpSwap();
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_801A7A88();

    static CAttributeHandler* fn_8009EF28( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CClumpSwap( packet );
    }

    unk_RpClump*    clump;      // 0x34, the copy of the packet's clump
    const char*     name;       // 0x38, points into the attribute packet
};

extern u32 lbl_805297C8;            // class-name hash of "CClumpSwap"
extern unk_801A8F6C lbl_805297CC;   // type-registry entry of CClumpSwap

#endif
