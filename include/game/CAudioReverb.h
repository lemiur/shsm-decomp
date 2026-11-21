#ifndef GAME_CAUDIOREVERB_H
#define GAME_CAUDIOREVERB_H

#include <climax/AttributeHandler.h>
#include <climax/Audio/System.h>
#include <climax/Memory.h>
#include <fmod/fmod.hpp>
#include <game/CBaseBehaviour.h>
#include <game/MusicTrigger.h>
#include <string.h>

// defined in 0x80037018, unit not yet reconstructed: hands the reverb name to the music player
void    fn_80037018( unk_MusicPlayer* player, const char* name );

/*
 * Switches the audio system's reverb while a message is in effect: the enter message sets the
 * named reverb preset (through the music player when the switch is not applied directly), the
 * exit message restores the reverb saved at the switch. Its attribute packet names the two
 * messages and the preset, and whether the reverb is enabled. Vtable lbl_803E2568, size 0x64.
 * Member names invented.
 */
class CAudioReverb : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetEnterMsg,        // text: the message that switches the reverb
        CMD_SetExitMsg,         // text: the message that restores the reverb
        CMD_SetReverbName,      // text: the name of the reverb preset
        CMD_SetEnabled          // integer: nonzero enables the switch on load
    };

                    CAudioReverb() {
                        enterMsg.fn_801A92FC( NULL );
                        exitMsg.fn_801A92FC( NULL );
                        reverbName = NULL;
                        savedReverb = NULL;
                        enabled = true;
                        memset( &reverbProps, 0, sizeof( reverbProps ) );
                    }
    virtual         ~CAudioReverb();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_8000CFD0();
    // nonzero applies the preset through Audio::System::SetReverb; zero hands the name to
    // fn_80037018 (the music player)
    virtual void    fn_8003BAE8( bool apply );
    // restores the saved preset, or the saved FMOD reverb properties when none was saved
    virtual void    fn_8003BB5C();

    static CAttributeHandler* fn_8003BBE4( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CAudioReverb();
    }

    bool                    enabled;        // 0x34
    unk_801A92A8            enterMsg;       // 0x38
    unk_801A92A8            exitMsg;        // 0x40
    const char*             reverbName;     // 0x48, points into the attribute packet
    FMOD_REVERB_PROPERTIES  reverbProps;    // 0x4C, the FMOD reverb saved at the switch
    Audio::ReverbPreset*    savedReverb;    // 0xC8, the preset current at the switch; NULL when none
};

extern u32 lbl_80529310;                    // class-name hash of "CAudioReverb"
extern unk_801A8F6C lbl_80529314;           // type-registry entry of CAudioReverb

#endif
