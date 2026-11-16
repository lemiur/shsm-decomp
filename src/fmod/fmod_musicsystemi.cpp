/*
 * fmod_musicsystemi.cpp: the Event system's music system (FMOD::MusicSystemI), the reverb settings it
 * applies to the music's channels (FMOD::MusicSettings) and the prompt for one cue (FMOD::CuePrompt).
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_musicsystemi.obj)

#include <new>
#include <string.h>

#include <fmod/fmod.hpp>
#include <fmod/fmod_compositioncore.h>
#include <fmod/fmod_compositionentities.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_musicengine.h>
#include <fmod/fmod_musicsystemi.h>

namespace FMOD {

// Returns from the enclosing function when a call fails.
#define CHECK_RESULT( _call )              \
    {                                      \
        FMOD_RESULT _result = ( _call );   \
        if ( _result != FMOD_OK ) {        \
            return _result;                \
        }                                  \
    }

/*
====================
MusicPromptI::isActive

Whether the prompt is playing.
====================
*/
FMOD_RESULT MusicPromptI::isActive( bool *active ) {
    if ( !active ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *active = m_active;

    return FMOD_OK;
} // 0x803AC1EC

/*
====================
MusicPromptI::getMemoryInfo
====================
*/
FMOD_RESULT MusicPromptI::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                         unsigned int *memoryused_details ) {
    return FMOD_ERR_UNIMPLEMENTED;
} // 0x803AC20C

/*
====================
CuePrompt::release

Frees the prompt.
====================
*/
FMOD_RESULT CuePrompt::release() {
#line 31
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x803AC214

/*
====================
CuePrompt::begin

Starts the cue in the scene on top of the primary state's scene stack, and its theme if the scene has one.
====================
*/
FMOD_RESULT CuePrompt::begin() {
    if ( m_active ) {
        return FMOD_OK;
    }

    if ( !m_engine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    unsigned int scene_id = m_engine->m_primary_state.getSceneStackTop();
    Scene *scene          = SceneRepository::repository()->getScene( scene_id );
    if ( !scene ) {
        return FMOD_ERR_INTERNAL;
    }

    CHECK_RESULT( m_engine->beginCue( m_cue_id ) );

    unsigned int theme_id = scene->getThemeForCue( m_cue_id );
    if ( theme_id ) {
        CHECK_RESULT( m_engine->beginTheme( theme_id, &m_themestack_entry ) );
    }

    m_active = true;

    return FMOD_OK;
} // 0x803AC254

/*
====================
CuePrompt::end

Ends the cue and the theme it began.
====================
*/
FMOD_RESULT CuePrompt::end() {
    if ( !m_active ) {
        return FMOD_OK;
    }

    if ( !m_engine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    CHECK_RESULT( m_engine->endTheme( m_themestack_entry ) );
    m_themestack_entry.m_state    = 0;
    m_themestack_entry.m_stack_id = 0;

    CHECK_RESULT( m_engine->endCue( m_cue_id ) );

    m_active = false;

    return FMOD_OK;
} // 0x803AC34C

/*
====================
gainTodB

A gain to decibels: -60 dB at and below a gain of 0.001. Inlined at every call; the link drops the
out-of-line copy (no copy in retail). Its constants come first in .sdata2 because it is compiled before
the constructor; the dB result is a local, without which MWCC folds the two returns into a select and
pools the threshold last.
====================
*/
float gainTodB( float gain ) {
    if ( gain <= 0.001f ) {
        return -60.0f;
    }

    float dB = 20.0f * (float)log10( gain );
    return dB;
} // inlined (dead-stripped)

/*
====================
MusicSettings::MusicSettings

Starts with every instance of the channel reverb on its default properties.
====================
*/
// The defaults are an unnamed aggregate (a compound literal, an MWCC extension in C++): Gormiti's DWARF names only
// cprop in the loop body, and its line table puts the copy of the defaults and the memcpy on one source line.
MusicSettings::MusicSettings() {
    m_basevolume       = 1.0f;
    m_basereverblevel  = 1.0f;
    m_reverbdrylevelmb = 0;
    m_reverbwetlevelmb = 0;
    m_channelgroup     = 0;
    for ( int instance = 0; instance < 4; instance++ ) {
        FMOD_REVERB_CHANNELPROPERTIES cprop;

        memcpy( &cprop,
                &(FMOD_REVERB_CHANNELPROPERTIES){ 0, 0, 0, 0, 0, 0.0f, 0, 0.25f, 1.5f, 1.0f, 0, 1.0f, 0, 1.0f, 0.0f, 0.0f, 0.0f,
                                                  FMOD_REVERB_CHANNELFLAGS_DIRECTHFAUTO | FMOD_REVERB_CHANNELFLAGS_ROOMAUTO |
                                                      FMOD_REVERB_CHANNELFLAGS_ROOMHFAUTO | FMOD_REVERB_CHANNELFLAGS_INSTANCE0,
                                                  0 },
                sizeof( cprop ) );

        cprop.Flags &= ~FMOD_REVERB_CHANNELFLAGS_INSTANCE0;
        cprop.Flags |= FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance;
        cprop.Direct += m_reverbdrylevelmb;
        cprop.Room += m_reverbwetlevelmb;

        memcpy( &m_reverbproperties[instance], &cprop, sizeof( cprop ) );
    }
} // 0x803AC3E4

/*
====================
MusicSettings::setChannelGroup

Sets the group whose reverb the settings override and applies every instance to it.
====================
*/
FMOD_RESULT MusicSettings::setChannelGroup( ChannelGroup *group ) {
    m_channelgroup = group;

    if ( m_channelgroup ) {
        for ( int instance = 0; instance < 4; instance++ ) {
            CHECK_RESULT( m_channelgroup->overrideReverbProperties( &m_reverbproperties[instance] ) );
        }
    }

    return FMOD_OK;
} // 0x803AC4E4

/*
====================
MusicSettings::applyReverbProperties

Applies every instance to a channel; a channel that has no reverb instance to set is not an error.
====================
*/
FMOD_RESULT MusicSettings::applyReverbProperties( Channel *channel ) {
    for ( int instance = 0; instance < 4; instance++ ) {
        FMOD_RESULT result = channel->setReverbProperties( &m_reverbproperties[instance] );
        if ( result != FMOD_ERR_REVERB_INSTANCE && result != FMOD_OK ) {
            return result;
        }
    }

    return FMOD_OK;
} // 0x803AC55C

/*
====================
MusicSettings::setReverbProperties

Applies the properties, offset by the base levels, to the instances the flags select (the first when
none is selected); the other instances take only the new direct level.
====================
*/
FMOD_RESULT MusicSettings::setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    int instance;
    int numinstances = 0;
    FMOD_REVERB_CHANNELPROPERTIES cprop;

    if ( !prop ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    for ( instance = 0; instance < 4; instance++ ) {
        if ( prop->Flags & ( FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance ) ) {
            numinstances++;
        }
    }

    memcpy( &cprop, prop, sizeof( cprop ) );

    cprop.Direct += m_reverbdrylevelmb;
    cprop.Room += m_reverbwetlevelmb;
    cprop.ConnectionPoint = 0;

    for ( instance = 0; instance < 4; instance++ ) {
        if ( ( prop->Flags & ( FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance ) ) || ( numinstances == 0 && instance == 0 ) ) {
            memcpy( &m_reverbproperties[instance], &cprop, sizeof( cprop ) );
            m_reverbproperties[instance].Flags &= 0xFFFFFF0F;
            m_reverbproperties[instance].Flags |= FMOD_REVERB_CHANNELFLAGS_INSTANCE0 << instance;
        } else {
            m_reverbproperties[instance].Direct = cprop.Direct;
        }
    }

    if ( m_channelgroup ) {
        CHECK_RESULT( m_channelgroup->overrideReverbProperties( &cprop ) );
    }

    return FMOD_OK;
} // 0x803AC5D0

/*
====================
MusicSettings::getReverbProperties

Returns the first instance the flags select (the first when none is selected), without the base levels.
====================
*/
FMOD_RESULT MusicSettings::getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    if ( !prop ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    // The first instance the flags select.
    memcpy( prop,
            &m_reverbproperties[( prop->Flags & FMOD_REVERB_CHANNELFLAGS_INSTANCE0 )   ? 0
                                : ( prop->Flags & FMOD_REVERB_CHANNELFLAGS_INSTANCE1 ) ? 1
                                : ( prop->Flags & FMOD_REVERB_CHANNELFLAGS_INSTANCE2 ) ? 2
                                : ( prop->Flags & FMOD_REVERB_CHANNELFLAGS_INSTANCE3 ) ? 3
                                                                                         : 0],
            sizeof( *prop ) );

    prop->Direct -= m_reverbdrylevelmb;
    prop->Room -= m_reverbwetlevelmb;

    return FMOD_OK;
} // 0x803AC734

/*
====================
MusicSettings::setBaseVolume

Moves every instance from the old base levels to the ones the new volume gives.
====================
*/
FMOD_RESULT MusicSettings::setBaseVolume( float volume ) {
    m_basevolume = volume;

    for ( int instance = 0; instance < 4; instance++ ) {
        m_reverbproperties[instance].Direct -= m_reverbdrylevelmb;
        m_reverbproperties[instance].Room -= m_reverbwetlevelmb;
    }

    m_reverbdrylevelmb = dBToReverbLevel( gainTodB( m_basevolume ) );
    m_reverbwetlevelmb = dBToReverbLevel( gainTodB( m_basevolume * m_basereverblevel ) );

    for ( int instance = 0; instance < 4; instance++ ) {
        m_reverbproperties[instance].Direct += m_reverbdrylevelmb;
        m_reverbproperties[instance].Room += m_reverbwetlevelmb;

        if ( m_channelgroup ) {
            CHECK_RESULT( m_channelgroup->overrideReverbProperties( &m_reverbproperties[instance] ) );
        }
    }

    return FMOD_OK;
} // 0x803AC7F4

/*
====================
MusicSettings::getBaseVolume
====================
*/
FMOD_RESULT MusicSettings::getBaseVolume( float *volume ) const {
    *volume = m_basevolume;

    return FMOD_OK;
} // 0x803AC978

/*
====================
MusicSettings::setBaseReverbLevel

Moves every instance from the old wet level to the one the new reverb level gives.
====================
*/
FMOD_RESULT MusicSettings::setBaseReverbLevel( float level ) {
    m_basereverblevel = level;

    for ( int instance = 0; instance < 4; instance++ ) {
        m_reverbproperties[instance].Room -= m_reverbwetlevelmb;
    }

    m_reverbwetlevelmb = dBToReverbLevel( gainTodB( m_basevolume * m_basereverblevel ) );

    for ( int instance = 0; instance < 4; instance++ ) {
        m_reverbproperties[instance].Room += m_reverbwetlevelmb;

        if ( m_channelgroup ) {
            CHECK_RESULT( m_channelgroup->overrideReverbProperties( &m_reverbproperties[instance] ) );
        }
    }

    return FMOD_OK;
} // 0x803AC988

/*
====================
MusicSettings::getBaseReverbLevel
====================
*/
FMOD_RESULT MusicSettings::getBaseReverbLevel( float *level ) const {
    *level = m_basereverblevel;

    return FMOD_OK;
} // 0x803ACA94

/*
====================
MusicSystemI::MusicSystemI
====================
*/
MusicSystemI::MusicSystemI() {
    m_musicengine   = 0;
    m_system        = 0;
    m_musicsettings = 0;
} // 0x803ACAA4

/*
====================
MusicSystemI::init
====================
*/
FMOD_RESULT MusicSystemI::init( System *system ) {
    m_system = system;

    return FMOD_OK;
} // 0x803ACAC4

/*
====================
MusicSystemI::initMusicEngine

Creates the engine (and the settings, unless the caller gives some), starts it and hangs its channel group
under group.
====================
*/
FMOD_RESULT MusicSystemI::initMusicEngine( ChannelGroup *group, MusicSettings *settings ) {
#line 397
    m_musicengine = new ( FMOD_Memory_Alloc( sizeof( MusicEngine ) ) ) MusicEngine;
    if ( !m_musicengine ) {
        return FMOD_ERR_MEMORY;
    }

    if ( !settings ) {
#line 405
        settings = new ( FMOD_Memory_Alloc( sizeof( MusicSettings ) ) ) MusicSettings;
        if ( !settings ) {
            return FMOD_ERR_MEMORY;
        }
    }

    m_musicsettings = settings;

    CHECK_RESULT( m_musicengine->init( m_system, 10, settings ) );

    if ( group && m_musicengine->m_channelgroup ) {
        CHECK_RESULT( group->addGroup( m_musicengine->m_channelgroup ) );
    }

    CHECK_RESULT( m_musicengine->start() );

    return FMOD_OK;
} // 0x803ACAD0

/*
====================
MusicSystemI::update
====================
*/
FMOD_RESULT MusicSystemI::update() {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->update() );
    }

    return FMOD_OK;
} // 0x803ACC0C

/*
====================
MusicSystemI::release

Frees the settings, stops and releases the engine, and frees the music system.
====================
*/
FMOD_RESULT MusicSystemI::release() {
    if ( m_musicsettings ) {
#line 441
        FMOD_Memory_Free( m_musicsettings );
        m_musicsettings = 0;
    }

    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->stop() );
        CHECK_RESULT( m_musicengine->release() );
    }

#line 451
    FMOD_Memory_Free( this );

    return FMOD_OK;
} // 0x803ACC48

/*
====================
MusicSystemI::ready

Whether the engine has been created.
====================
*/
bool MusicSystemI::ready() {
    return m_musicengine != 0;
} // 0x803ACCF4

/*
====================
MusicSystemI::reset
====================
*/
FMOD_RESULT MusicSystemI::reset() {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->reset() );
    }

    return FMOD_OK;
} // 0x803ACD08

/*
====================
MusicSystemI::setVolume
====================
*/
FMOD_RESULT MusicSystemI::setVolume( float volume ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->setVolume( volume ) );
    }

    return FMOD_OK;
} // 0x803ACD44

/*
====================
MusicSystemI::getVolume
====================
*/
FMOD_RESULT MusicSystemI::getVolume( float *volume ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->getVolume( volume ) );
    }

    return FMOD_OK;
} // 0x803ACD80

/*
====================
MusicSystemI::setReverbProperties
====================
*/
FMOD_RESULT MusicSystemI::setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicsettings->setReverbProperties( prop ) );
    }

    return FMOD_OK;
} // 0x803ACDBC

/*
====================
MusicSystemI::getReverbProperties
====================
*/
FMOD_RESULT MusicSystemI::getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicsettings->getReverbProperties( prop ) );
    }

    return FMOD_OK;
} // 0x803ACE08

/*
====================
MusicSystemI::setPaused
====================
*/
FMOD_RESULT MusicSystemI::setPaused( bool paused ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->setPaused( paused ) );
    }

    return FMOD_OK;
} // 0x803ACE54

/*
====================
MusicSystemI::getPaused
====================
*/
FMOD_RESULT MusicSystemI::getPaused( bool *paused ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->getPaused( paused ) );
    }

    return FMOD_OK;
} // 0x803ACE90

/*
====================
MusicSystemI::setMute
====================
*/
FMOD_RESULT MusicSystemI::setMute( bool mute ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->setMute( mute ) );
    }

    return FMOD_OK;
} // 0x803ACECC

/*
====================
MusicSystemI::getMute
====================
*/
FMOD_RESULT MusicSystemI::getMute( bool *mute ) {
    if ( m_musicengine ) {
        CHECK_RESULT( m_musicengine->getMute( mute ) );
    }

    return FMOD_OK;
} // 0x803ACF08

/*
====================
MusicSystemI::getInfo
====================
*/
FMOD_RESULT MusicSystemI::getInfo( FMOD_MUSIC_INFO *info ) {
    if ( !info ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    CHECK_RESULT( m_musicengine->getInfo( info ) );

    return FMOD_OK;
} // 0x803ACF44

/*
====================
MusicSystemI::promptCue

Plays a cue: prepares a prompt for it, begins and ends it, and releases it.
====================
*/
FMOD_RESULT MusicSystemI::promptCue( unsigned int id ) {
    if ( !m_musicengine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    MusicPrompt *prompt;

    CHECK_RESULT( prepareCue( id, &prompt ) );
    CHECK_RESULT( prompt->begin() );
    CHECK_RESULT( prompt->end() );
    CHECK_RESULT( prompt->release() );

    return FMOD_OK;
} // 0x803ACF8C

/*
====================
MusicSystemI::prepareCue

Creates a prompt for a cue.
====================
*/
FMOD_RESULT MusicSystemI::prepareCue( unsigned int id, MusicPrompt **prompt ) {
    if ( !prompt ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *prompt = 0;

    if ( !m_musicengine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

#line 608
    CuePrompt *cueprompt = new ( FMOD_Memory_Alloc( sizeof( CuePrompt ) ) ) CuePrompt;
    if ( !cueprompt ) {
        return FMOD_ERR_MEMORY;
    }

    FMOD_RESULT result = cueprompt->init( id, m_musicengine );

    if ( result != FMOD_OK ) {
        return cueprompt->release();
    }

    *prompt = cueprompt;

    return FMOD_OK;
} // 0x803AD044

/*
====================
MusicSystemI::getParameterValue
====================
*/
FMOD_RESULT MusicSystemI::getParameterValue( unsigned int id, float *parameter ) {
    if ( !parameter ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *parameter = 0.0f;

    if ( !m_musicengine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    Parameter *param = ParameterRepository::repository()->getParameter( id );
    if ( !param ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    *parameter = param->getValue();

    return FMOD_OK;
} // 0x803AD144

/*
====================
MusicSystemI::setParameterValue
====================
*/
FMOD_RESULT MusicSystemI::setParameterValue( unsigned int id, float parameter ) {
    if ( !m_musicengine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    Parameter *param = ParameterRepository::repository()->getParameter( id );
    if ( !param ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    param->setValue( parameter );

    return FMOD_OK;
} // 0x803AD1E4

/*
====================
MusicSystemI::getCues

Starts an enumeration of the cues whose names match the filter.
====================
*/
FMOD_RESULT MusicSystemI::getCues( FMOD_MUSIC_ITERATOR *it, const char *filter ) {
    if ( !it ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    it->filter = filter;

    if ( !m_musicengine ) {
        it->value = 0;
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    MusicEntityListing listing;

    listing = CueRepository::repository()->getMusicEntityListing();

    it->value = listing.find( listing.m_listing, filter );

    return FMOD_OK;
} // 0x803AD26C

/*
====================
MusicSystemI::getNextCue

Steps an enumeration of cues to the next match.
====================
*/
FMOD_RESULT MusicSystemI::getNextCue( FMOD_MUSIC_ITERATOR *it ) {
    if ( !it ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !m_musicengine ) {
        it->value = 0;
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    MusicEntityListing listing;

    listing = CueRepository::repository()->getMusicEntityListing();

    if ( !listing.validate( it->value ) ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    it->value = listing.find( it->value + 1, it->filter );

    return FMOD_OK;
} // 0x803AD308

/*
====================
MusicSystemI::getParameters

Starts an enumeration of the parameters whose names match the filter.
====================
*/
FMOD_RESULT MusicSystemI::getParameters( FMOD_MUSIC_ITERATOR *it, const char *filter ) {
    if ( !it ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    it->filter = filter;

    if ( !m_musicengine ) {
        it->value = 0;
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    MusicEntityListing listing;

    listing = ParameterRepository::repository()->getMusicEntityListing();

    it->value = listing.find( listing.m_listing, filter );

    return FMOD_OK;
} // 0x803AD3B8

/*
====================
MusicSystemI::getNextParameter

Steps an enumeration of parameters to the next match.
====================
*/
FMOD_RESULT MusicSystemI::getNextParameter( FMOD_MUSIC_ITERATOR *it ) {
    if ( !it ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( !m_musicengine ) {
        it->value = 0;
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    MusicEntityListing listing;

    listing = ParameterRepository::repository()->getMusicEntityListing();

    if ( !listing.validate( it->value ) ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    it->value = listing.find( it->value + 1, it->filter );

    return FMOD_OK;
} // 0x803AD454

/*
====================
MusicSystemI::loadSoundData
====================
*/
FMOD_RESULT MusicSystemI::loadSoundData( FMOD_EVENT_RESOURCE resource, unsigned int mode ) {
    if ( !m_musicengine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    CHECK_RESULT( SegmentRepository::repository()->loadSoundData( resource, mode ) );

    return FMOD_OK;
} // 0x803AD504

/*
====================
MusicSystemI::freeSoundData
====================
*/
FMOD_RESULT MusicSystemI::freeSoundData( bool waituntilready ) {
    if ( !m_musicengine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    CHECK_RESULT( SegmentRepository::repository()->freeSoundData( waituntilready ) );

    return FMOD_OK;
} // 0x803AD57C

/*
====================
MusicSystemI::setCallback
====================
*/
FMOD_RESULT MusicSystemI::setCallback( FMOD_MUSIC_CALLBACK callback, void *userdata ) {
    if ( !m_musicengine ) {
        return FMOD_ERR_MUSIC_UNINITIALIZED;
    }

    CHECK_RESULT( m_musicengine->setCallback( callback, userdata ) );

    return FMOD_OK;
} // 0x803AD5E4

/*
====================
MusicSystemI::getMemoryInfo
====================
*/
FMOD_RESULT MusicSystemI::getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                         unsigned int *memoryused_details ) {
    return FMOD_ERR_UNIMPLEMENTED;
} // 0x803AD62C

} // namespace FMOD
