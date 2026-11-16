#ifndef GAME_CBUTTONPUZZLE_H
#define GAME_CBUTTONPUZZLE_H

// unit name unconfirmed: the class name is the registration string "CButtonPuzzle"; the file name
// is invented.

#include <climax/Memory.h>
#include <game/unk_80161958.h>

/*
 * The button puzzle (registered as "CButtonPuzzle", vtable lbl_803F85A8, size 0x5C8): a puzzle
 * behaviour that can fade its clump out over a set time once it is solved. Member names invented.
 */
class CButtonPuzzle : public unk_80161958 {
public:
                    CButtonPuzzle( const unk_AttributePacket& packet ) :
                        unk_80161958( packet ), field_0x5BC( false ), field_0x5C4( 0.0f ) {}
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual void    fn_80164578();  // override
    virtual void    fn_80165B90( bool reset );  // override
    virtual void    fn_8015DBD4();  // override
    virtual u32     fn_8015AE1C( u32* out );  // override

    static CAttributeHandler* fn_8015AFB4( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CButtonPuzzle( packet );
    }

    bool            field_0x5BC;
    bool            field_0x5BD;    // fade out once solved
    float           field_0x5C0;    // the fade time
    float           field_0x5C4;    // the fade time left
};

extern u32 lbl_8052A170;            // class-name hash of "CButtonPuzzle"
extern unk_801A8F6C lbl_8052A174;   // type-registry entry of CButtonPuzzle

#endif
