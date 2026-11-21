// Unit name and boundary: Gormiti link map (game/Objects/Audio/AudioMaterialMap.cpp)

#include <game/AudioMaterialMap.h>
#include <climax/Heap.h>

// the registered class id of "MaterialMap", read by HandleAttributes
static u32 lbl_80529300 = fn_801A7A84( "MaterialMap" );
// the factory registration of the class (instance size 0x80)
static unk_801A8F6C lbl_80529304( "MaterialMap", Audio::MaterialMap::MakeNew, 0x80 );

// the instance pointer, declared in the header
Audio::MaterialMap* lbl_80529308;

/*
====================
Audio::MaterialMap::Instance

Returns the material map held in lbl_80529308.
====================
*/
// 0x80039B04
Audio::MaterialMap& Audio::MaterialMap::Instance() {
    return *lbl_80529308;
}

/*
====================
Audio::MaterialMap::~MaterialMap
====================
*/
// 0x80039B0C
Audio::MaterialMap::~MaterialMap() {
    lbl_80529308 = 0;
}

/*
====================
Audio::MaterialMap::MakeNew
====================
*/
// 0x80039DB8
CAttributeHandler* Audio::MaterialMap::MakeNew( const unk_AttributePacket& packet ) {
    return new ( GAME_HEAP, GAME_HEAP_FLAGS ) MaterialMap;
}

/*
====================
Audio::MaterialMap::fn_801A73A8
====================
*/
// 0x80039B6C
void Audio::MaterialMap::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    for ( unk_AttributeCommandIterator it( packet, lbl_80529300 ); !it.IsFinished(); ++it ) {
        // the surface type the command's cue applies to; 0x26 is no surface
        int surface = 0x26;
        switch ( it->id ) {
        case 0:  surface = 0x0;  break;
        case 1:  surface = 0x24; break;
        case 2:  surface = 0x4;  break;
        case 3:  surface = 0x6;  break;
        case 4:  surface = 0x8;  break;
        case 5:  surface = 0xA;  break;
        case 6:  surface = 0xC;  break;
        case 7:  surface = 0xE;  break;
        case 8:  surface = 0x10; break;
        case 9:  surface = 0x12; break;
        case 10: surface = 0x14; break;
        case 11: surface = 0x16; break;
        case 12: surface = 0x18; break;
        case 13: surface = 0x1A; break;
        case 14: surface = 0x1C; break;
        case 15: surface = 0x1E; break;
        case 16: surface = 0x20; break;
        case 17: surface = 0x22; break;
        }
        if ( surface != 0x26 ) {
            m_cueNames[ surface / 2 ] = it->GetString();
        }
    }
}

/*
====================
Audio::MaterialMap::fn_8009CB1C
====================
*/
// 0x80039CBC
void Audio::MaterialMap::fn_8009CB1C( unk_Msg& msg ) {
}

/*
====================
fn_80039CC0
====================
*/
// 0x80039CC0
extern "C" void fn_80039CC0() {
    // TODO: PlayFootStep; the relay is a fixed offset of the object RespotManager::fn_80030254 returns
}

