#ifndef GAME_AUDIOMATERIALMAP_H
#define GAME_AUDIOMATERIALMAP_H

#include <string.h>

#include <climax/AttributeHandler.h>
#include <game/CBaseBehaviour.h>

namespace Audio {
class MaterialMap;
}

// the instance registered by the constructor and cleared by the destructor; read by Instance(). Name invented.
extern Audio::MaterialMap* lbl_80529308;

namespace Audio {

/*
 * The material map of the Audio system: the cue a footstep plays on each surface type, read from
 * the attribute packet. Name from Gormiti's Audio::MaterialMap (game/Objects/Audio/AudioMaterialMap.cpp);
 * its instance is 0x80 bytes here (the size the factory registers), 0xA4 in Gormiti.
 */
class MaterialMap : public CBaseBehaviour {
public:
    // the constructor is inline: only the factory MakeNew constructs one
    MaterialMap() {
        lbl_80529308 = this;
        memset( m_cueNames, 0, sizeof( m_cueNames ) );
    }
    // the first virtual defined out of line here is the key function: it makes this unit emit the vtable
    // overrides CAttributeHandler::fn_801A73A8 (Gormiti: HandleAttributes)
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~MaterialMap();
    static MaterialMap& Instance();
    // registered as the factory of the class (the class name "MaterialMap")
    static CAttributeHandler* MakeNew( const unk_AttributePacket& packet );

    // overrides CEventHandler::fn_8009CB1C in the second base at 0x24 (Gormiti: HandleEvents)
    virtual void    fn_8009CB1C( unk_Msg& msg );

    // the cue names per surface pair, one slot per surface type divided by two: slot 18 holds
    // the surface types 0x24 and 0x25
    const char*     m_cueNames[ 19 ];   // 0x34
};

}   // namespace Audio

#endif
