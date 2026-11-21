#ifndef GAME_UNK_800338DC_H
#define GAME_UNK_800338DC_H

#include <climax/AttributeHandler.h>
#include <climax/Audio/Defs.h>
#include <climax/EventHandler.h>
#include <game/CBaseBehaviour.h>

namespace Audio {
class CAudioRelay {
public:
    // defined in 0x8003A898, unit not yet reconstructed; returns the handle of the sound it starts
    s32         PlaySound( const char* name, const unk_RwV3d* position, Channel::Type channel,
                           bool loop, bool paused );
};
}

// The audio relay a zone holds at 0x34 (CZone is not reconstructed yet). Name invented.
Audio::CAudioRelay* GetZoneAudioRelay( CZone* zone );

// The registered class id of this class, defined in the unit after this one (not reconstructed).
extern u32 lbl_80529290;

extern "C" {
// defined in 0x8003AFDC, unit not yet reconstructed: stops the sound the relay plays
void fn_8003AFDC( Audio::CAudioRelay* relay );
// defined in 0x8003B1B0, unit not yet reconstructed: sets the volume of the sound handle
void fn_8003B1B0( Audio::CAudioRelay* relay, s32 sound, bool loop, float volume );
}

/*
 * A behaviour that plays a sound at its position while the player is near: attribute 0 names
 * the message that starts the sound (startEvent), attribute 1 the one that stops it, attribute
 * 2 the sound's name (copied; freed by the destructor), attributes 3 and 4 the distances over
 * which its volume rises from 0 to 1 as the player moves away. The frame message refits the
 * volume. Vtable lbl_803E1F88 (size 0x58), constructor 0x800338DC, destructor 0x800339A4, the
 * attribute reader 0x80033A6C, the message handler 0x80033B70. Name invented.
 */
class unk_800338DC : public CBaseBehaviour {
public:
    enum {
        CMD_SetStartEvent,      // text: the message that starts the sound
        CMD_SetStopEvent,       // text: the message that stops it
        CMD_SetSoundName,       // text: the sound
        CMD_SetNearDistance,    // float: the distance at volume 0
        CMD_SetFarDistance      // float: the distance at volume 1
    };

                    unk_800338DC();
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~unk_800338DC();  // override

    unk_801A92A8    startEvent;     // 0x34
    unk_801A92A8    stopEvent;      // 0x3C
    char*           soundName;      // 0x44, owned
    float           nearDistance;   // 0x48
    float           farDistance;    // 0x4C
    s32             sound;          // 0x50, the handle of the playing sound; 0 while none plays
};

#endif
