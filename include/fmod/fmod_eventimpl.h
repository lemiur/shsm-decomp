/*
 * FMOD::EventImpl: the body of an event (EventI::m_impl), in a simple form (one sound definition,
 * EventImplSimple) or a complex one (layers, parameters, envelopes: EventImplComplex, not declared
 * yet). EventImpl's vptr follows its first member.
 * Virtuals: retail vtables __vt__Q24FMOD9EventImpl (init and 52 pure slots) and
 * __vt__Q24FMOD15EventImplSimple + Gormiti link map; EventImpl has no destructor slot.
 * Layout: Gormiti DWARF (sibling.exe types EventImpl, EventImplSimple).
 */
#ifndef _FMOD_EVENTIMPL_H
#define _FMOD_EVENTIMPL_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>

namespace FMOD {

class ChannelGroup;
class EventI;
class EventParameterDef;
class EventParameterI;
class EventSound;
class File;
class SimpleMemPool;
class SoundDef;

// size 0x8
class EventImpl {
public:
    EventI *m_eventi;   // 0x00, the event this is the body of

    EventImpl() { m_eventi = NULL; }

    // vtable pointer at 0x04: the first virtual below
    virtual FMOD_RESULT init( EventI *eventi );                                         // slot 0x08
    virtual FMOD_RESULT release() = 0;
    virtual FMOD_RESULT releaseData() = 0;
    virtual FMOD_RESULT unloadAllSounds() = 0;
    virtual FMOD_RESULT updateTimeParameters( unsigned int delta ) = 0;
    virtual FMOD_RESULT update( unsigned int elapsed_time, unsigned int updateflags ) = 0;
    virtual FMOD_RESULT createDSPNetwork() = 0;
    virtual FMOD_RESULT releaseDSPNetwork() = 0;
    virtual FMOD_RESULT createStreams( unsigned int eventmode ) = 0;
    virtual FMOD_RESULT stop() = 0;
    virtual FMOD_RESULT buildSoundBankList() = 0;
    virtual FMOD_RESULT updateEnvelopes( void *data ) = 0;
    virtual FMOD_RESULT getLengthMs( int *length ) = 0;
    virtual FMOD_RESULT getPositionMs( unsigned int *position ) = 0;
    virtual FMOD_RESULT getChannelsPlaying( int *channelsplaying ) = 0;
    virtual FMOD_RESULT getAudibility( float *volume ) = 0;
    virtual FMOD_RESULT estimateAudibility( float *audibility ) = 0;
    virtual FMOD_RESULT start() = 0;
    virtual FMOD_RESULT getParameter( const char *name, EventParameter **parameter ) = 0;
    virtual FMOD_RESULT getParameterByIndex( int index, EventParameter **parameter ) = 0;
    virtual FMOD_RESULT getParameterWithDef( EventParameterDef *def, EventParameterI **parameter ) = 0;
    virtual FMOD_RESULT getNumParameters( int *numparameters ) = 0;
    virtual FMOD_RESULT getProperty( const char *propertyname, void *value ) = 0;
    virtual FMOD_RESULT getPropertyByIndex( int propertyindex, void *value ) = 0;
    virtual FMOD_RESULT getNumProperties( int *numproperties ) = 0;
    virtual FMOD_RESULT getPropertyInfo( int *propertyindex, char **propertyname, FMOD_EVENTPROPERTY_TYPE *type ) = 0;
    virtual FMOD_RESULT getState( unsigned int *state ) = 0;
    virtual FMOD_RESULT willTerminate( int *terminate ) const = 0;
    virtual FMOD_RESULT getParameterByIndexInternal( int index, EventParameterI **parameter ) = 0;
    virtual FMOD_RESULT duplicateEvent( EventI *dstevent, SimpleMemPool *mempool ) = 0;
    virtual FMOD_RESULT copyProperties( EventI *dstevent ) = 0;
    virtual FMOD_RESULT setupInstance() = 0;
    virtual FMOD_RESULT load( File *file, unsigned int fileversion ) = 0;
    virtual FMOD_RESULT dereferenceSoundnames( SoundDef *sounddeflisthead, int fileversion ) = 0;
    virtual FMOD_RESULT soundJustPlayed( EventSound *sound ) = 0;
    virtual FMOD_RESULT soundJustStopped( EventSound *sound ) = 0;
    virtual FMOD_RESULT useSoftware( EventSound *sound, bool *software ) = 0;
    virtual FMOD_RESULT getPriority( EventSound *sound, int *priority ) = 0;
    virtual FMOD_RESULT getChannelGroup( EventSound *sound, ChannelGroup **channelgroup ) = 0;
    virtual FMOD_RESULT hasPanEnvelope( EventSound *sound, bool *has ) = 0;
    virtual FMOD_RESULT hasSpeakerLevelEnvelope( EventSound *sound, bool *has ) = 0;
    virtual FMOD_RESULT getSound( int layerindex, int soundindex, EventSound **sound ) = 0;
    virtual FMOD_RESULT rescheduleSounds() = 0;
    virtual FMOD_RESULT clearPreviousSoundPointers( EventSound *sound ) = 0;
    virtual FMOD_RESULT setMode() = 0;
    virtual FMOD_RESULT setMinMaxDistance() = 0;
    virtual FMOD_RESULT setProperty( const char *propertyname, void *value ) = 0;
    virtual FMOD_RESULT setPropertyByIndex( int propertyindex, void *value ) = 0;
    virtual FMOD_RESULT set3DAttributes() = 0;
    virtual FMOD_RESULT setReverbProperties() = 0;
    virtual FMOD_RESULT getTimeOffset( EventSound *sound, float *timeoffset ) = 0;
    virtual FMOD_RESULT setReverbWetLevelDB() = 0;
    virtual FMOD_RESULT setReverbDryLevelDB() = 0;                                      // slot 0xD8
};

// An event with one sound definition. size 0x10
class EventImplSimple : public EventImpl {
public:
    unsigned int m_flags;   // 0x08
    EventSound  *m_sound;   // 0x0C, the event's sound

    EventImplSimple();

    // Every slot of EventImpl but init is overridden, in EventImpl's order.
    virtual FMOD_RESULT release();
    virtual FMOD_RESULT releaseData();
    virtual FMOD_RESULT unloadAllSounds();
    virtual FMOD_RESULT updateTimeParameters( unsigned int delta );
    virtual FMOD_RESULT update( unsigned int elapsed_time, unsigned int updateflags );
    virtual FMOD_RESULT createDSPNetwork();
    virtual FMOD_RESULT releaseDSPNetwork();
    virtual FMOD_RESULT createStreams( unsigned int eventmode );
    virtual FMOD_RESULT stop();
    virtual FMOD_RESULT buildSoundBankList();
    virtual FMOD_RESULT updateEnvelopes( void *data );
    virtual FMOD_RESULT getLengthMs( int *length );
    virtual FMOD_RESULT getPositionMs( unsigned int *position );
    virtual FMOD_RESULT getChannelsPlaying( int *channelsplaying );
    virtual FMOD_RESULT getAudibility( float *volume );
    virtual FMOD_RESULT estimateAudibility( float *audibility );
    virtual FMOD_RESULT start();
    virtual FMOD_RESULT getParameter( const char *name, EventParameter **parameter );
    virtual FMOD_RESULT getParameterByIndex( int index, EventParameter **parameter );
    virtual FMOD_RESULT getParameterWithDef( EventParameterDef *def, EventParameterI **parameter );
    virtual FMOD_RESULT getNumParameters( int *numparameters );
    virtual FMOD_RESULT getProperty( const char *propertyname, void *value );
    virtual FMOD_RESULT getPropertyByIndex( int propertyindex, void *value );
    virtual FMOD_RESULT getNumProperties( int *numproperties );
    virtual FMOD_RESULT getPropertyInfo( int *propertyindex, char **propertyname, FMOD_EVENTPROPERTY_TYPE *type );
    virtual FMOD_RESULT getState( unsigned int *state );
    virtual FMOD_RESULT willTerminate( int *terminate ) const;
    virtual FMOD_RESULT getParameterByIndexInternal( int index, EventParameterI **parameter );
    virtual FMOD_RESULT duplicateEvent( EventI *dstevent, SimpleMemPool *mempool );
    virtual FMOD_RESULT copyProperties( EventI *dstevent );
    virtual FMOD_RESULT setupInstance();
    virtual FMOD_RESULT load( File *file, unsigned int fileversion );
    virtual FMOD_RESULT dereferenceSoundnames( SoundDef *sounddeflisthead, int fileversion );
    virtual FMOD_RESULT soundJustPlayed( EventSound *sound );
    virtual FMOD_RESULT soundJustStopped( EventSound *sound );
    virtual FMOD_RESULT useSoftware( EventSound *sound, bool *software );
    virtual FMOD_RESULT getPriority( EventSound *sound, int *priority );
    virtual FMOD_RESULT getChannelGroup( EventSound *sound, ChannelGroup **channelgroup );
    virtual FMOD_RESULT hasPanEnvelope( EventSound *sound, bool *has );
    virtual FMOD_RESULT hasSpeakerLevelEnvelope( EventSound *sound, bool *has );
    virtual FMOD_RESULT getSound( int layerindex, int soundindex, EventSound **sound );
    virtual FMOD_RESULT rescheduleSounds();
    virtual FMOD_RESULT clearPreviousSoundPointers( EventSound *sound );
    virtual FMOD_RESULT setMode();
    virtual FMOD_RESULT setMinMaxDistance();
    virtual FMOD_RESULT setProperty( const char *propertyname, void *value );
    virtual FMOD_RESULT setPropertyByIndex( int propertyindex, void *value );
    virtual FMOD_RESULT set3DAttributes();
    virtual FMOD_RESULT setReverbProperties();
    virtual FMOD_RESULT getTimeOffset( EventSound *sound, float *timeoffset );
    virtual FMOD_RESULT setReverbWetLevelDB();
    virtual FMOD_RESULT setReverbDryLevelDB();

    // ---- added for fmod_eventimpl_simple (signature from the retail name; the second parameter is unnamed
    // in Gormiti's DWARF: unused) ----
    FMOD_RESULT updateSound( EventSound *sound, unsigned int delta );
};

} // namespace FMOD

#endif /* _FMOD_EVENTIMPL_H */
