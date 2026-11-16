/*
 * FMOD::MusicSystemI and its helpers: the Event system's music system, the settings it applies to the
 * music's channels, and a prompt for one cue. Header named after the Gormiti unit (fmod_musicsystemi.cpp).
 * Virtuals: retail vtables (__vt__Q24FMOD12MusicSystemI, __vt__Q24FMOD13MusicSettings,
 * __vt__Q24FMOD9CuePrompt) + Gormiti link map; layouts: Gormiti DWARF (sibling.exe types MusicSystemI,
 * MusicSettings, MusicPromptI, CuePrompt).
 * Class order: MWCC emits the vtables of a unit in reverse order of class declaration; retail's .data
 * (MusicSystemI, MusicSettings, CuePrompt) puts MusicSettings after the prompts and before MusicSystemI.
 */
#ifndef _FMOD_MUSICSYSTEMI_H
#define _FMOD_MUSICSYSTEMI_H

#include <math.h>

#include <fmod/fmod.h>
#include <fmod/fmod_compositionentities.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_musicengine.h>

namespace FMOD {

class Channel;
class ChannelGroup;
class System;

// Defined in the unit holding retail 0x80392ACC: a level in decibels to the reverb level in millibels.
int dBToReverbLevel( float dB );

// A gain to decibels; defined in fmod_musicsystemi.cpp (Gormiti link map: gainTodB__4FMODFf, unused there).
float gainTodB( float gain );

// The common part of a prompt: whether it is playing and the engine it plays in.
class MusicPromptI : public MusicPrompt {
public:
    MusicPromptI() : m_active( false ), m_engine( 0 ) {}
    void init( MusicEngine *engine ) {
        m_active = false;
        m_engine = engine;
    }

    virtual FMOD_RESULT isActive( bool *active );   // slot 0x14, override
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );  // slot 0x18, override

    bool m_active;                                  // 0x04
    MusicEngine *m_engine;                          // 0x08
}; // size 0xC

// A prompt for one cue.
class CuePrompt : public MusicPromptI {
public:
    CuePrompt() : m_cue_id( 0 ) {
        m_themestack_entry.m_state = 0;
        m_themestack_entry.m_stack_id = 0;
    }
    FMOD_RESULT init( unsigned int cue, MusicEngine *engine ) {
        MusicPromptI::init( engine );
        m_cue_id = cue;
        m_themestack_entry.m_state = 0;
        m_themestack_entry.m_stack_id = 0;
        return FMOD_OK;
    }

    virtual FMOD_RESULT release();                  // slot 0x08, override
    virtual FMOD_RESULT begin();                    // slot 0x0C, override
    virtual FMOD_RESULT end();                      // slot 0x10, override

    unsigned int m_cue_id;                          // 0x0C
    ThemeStackEntry m_themestack_entry;             // 0x10
}; // size 0x18

// The reverb the music's channels get: four instances of the channel properties, offset by a base
// volume and a base reverb level.
class MusicSettings {
public:
    MusicSettings();

    // vtable pointer at 0x00: the first virtual below
    virtual FMOD_RESULT setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );   // slot 0x08
    virtual FMOD_RESULT getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop );         // slot 0x0C
    virtual FMOD_RESULT setBaseVolume( float volume );                                      // slot 0x10
    virtual FMOD_RESULT getBaseVolume( float *volume ) const;                               // slot 0x14
    virtual FMOD_RESULT setBaseReverbLevel( float level );                                  // slot 0x18
    virtual FMOD_RESULT getBaseReverbLevel( float *level ) const;                           // slot 0x1C
    virtual FMOD_RESULT setChannelGroup( ChannelGroup *group );                             // slot 0x20
    virtual FMOD_RESULT applyReverbProperties( Channel *channel );                          // slot 0x24

    float m_basevolume;                             // 0x04
    float m_basereverblevel;                        // 0x08
    int m_reverbdrylevelmb;                         // 0x0C
    int m_reverbwetlevelmb;                         // 0x10
    FMOD_REVERB_CHANNELPROPERTIES m_reverbproperties[4];    // 0x14
    ChannelGroup *m_channelgroup;                   // 0x144
}; // size 0x148

class MusicSystemI : public MusicSystem {
public:
    MusicSystemI();

    FMOD_RESULT init( System *system );
    FMOD_RESULT initMusicEngine( ChannelGroup *group, MusicSettings *settings );
    FMOD_RESULT update();
    FMOD_RESULT release();
    bool ready();

    virtual FMOD_RESULT reset();                    // slot 0x08, override
    virtual FMOD_RESULT setVolume( float volume );  // slot 0x0C, override
    virtual FMOD_RESULT getVolume( float *volume ); // slot 0x10, override
    virtual FMOD_RESULT setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );  // slot 0x14, override
    virtual FMOD_RESULT getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop );        // slot 0x18, override
    virtual FMOD_RESULT setPaused( bool paused );   // slot 0x1C, override
    virtual FMOD_RESULT getPaused( bool *paused );  // slot 0x20, override
    virtual FMOD_RESULT setMute( bool mute );       // slot 0x24, override
    virtual FMOD_RESULT getMute( bool *mute );      // slot 0x28, override
    virtual FMOD_RESULT getInfo( FMOD_MUSIC_INFO *info );   // slot 0x2C, override
    virtual FMOD_RESULT promptCue( unsigned int id );       // slot 0x30, override
    virtual FMOD_RESULT prepareCue( unsigned int id, MusicPrompt **prompt );    // slot 0x34, override
    virtual FMOD_RESULT getParameterValue( unsigned int id, float *parameter ); // slot 0x38, override
    virtual FMOD_RESULT setParameterValue( unsigned int id, float parameter );  // slot 0x3C, override
    virtual FMOD_RESULT getCues( FMOD_MUSIC_ITERATOR *it, const char *filter );         // slot 0x40, override
    virtual FMOD_RESULT getNextCue( FMOD_MUSIC_ITERATOR *it );                          // slot 0x44, override
    virtual FMOD_RESULT getParameters( FMOD_MUSIC_ITERATOR *it, const char *filter );   // slot 0x48, override
    virtual FMOD_RESULT getNextParameter( FMOD_MUSIC_ITERATOR *it );                    // slot 0x4C, override
    virtual FMOD_RESULT loadSoundData( FMOD_EVENT_RESOURCE resource, unsigned int mode );   // slot 0x50, override
    virtual FMOD_RESULT freeSoundData( bool waituntilready );   // slot 0x54, override
    virtual FMOD_RESULT setCallback( FMOD_MUSIC_CALLBACK callback, void *userdata );    // slot 0x58, override
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );  // slot 0x5C, override

    MusicEngine *m_musicengine;                     // 0x04
    System *m_system;                               // 0x08
    MusicSettings *m_musicsettings;                 // 0x0C
}; // size 0x10

} // namespace FMOD

#endif /* _FMOD_MUSICSYSTEMI_H */
