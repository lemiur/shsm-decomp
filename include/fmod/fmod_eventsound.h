/*
 * FMOD::EventSound: one playing instance of a sound definition in an event (a layer's sound, or
 * the one sound of a simple event). Not polymorphic.
 * Layout: Gormiti DWARF (sibling.exe types EventSound), size 0x8C; retail's EventInstancePool
 * allocation (0x8C bytes) agrees. Only the methods used so far are declared.
 */
#ifndef _FMOD_EVENTSOUND_H
#define _FMOD_EVENTSOUND_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class Channel;
class DSP;
class EventI;
class EventLayer;
class File;
class SimpleMemPool;
class SoundDef;
class Sound;

// EventSound::m_flags bits (names invented from use; unknown bits keep their value).
#define EVENTSOUND_FLAG_0x01         0x0001  // cleared on a spawned sound when EventImplSimple::stop unloads it
#define EVENTSOUND_FLAG_0x02         0x0002  // unloaded directly rather than through its spawned sounds
#define EVENTSOUND_FLAG_ONESHOT      0x0004  // a oneshot: spawns sounds (createOneshotSounds)
#define EVENTSOUND_FLAG_0x10         0x0010  // as 0x02
#define EVENTSOUND_FLAG_START_CLEAR  0x0189  // the bits start() clears
#define EVENTSOUND_FLAG_0x400        0x0400  // set by start()
// ---- added for fmod_eventimpl_simple ----
#define EVENTSOUND_FLAG_0x08         0x0008  // with 0x10: EventImplSimple::update checks the channel still plays
#define EVENTSOUND_FLAG_0x80         0x0080  // counts as playing in EventImplSimple::update
#define EVENTSOUND_FLAG_0x100        0x0100  // set once the sound has been started (EventImplSimple::update)

// size 0x34 (Gormiti DWARF: sibling.exe types EventSoundDef)
class EventSoundDef {
public:
    int          m_refcnt;               // 0x00
    SoundDef    *m_sounddef;             // 0x04
    char        *m_sounddefname;         // 0x08
    float        m_x;                    // 0x0C
    float        m_width;                // 0x10
    float        m_volume;               // 0x14
    float        m_fadein;               // 0x18
    float        m_fadeout;              // 0x1C
    float        m_autopitchreference;   // 0x20
    float        m_autopitchatzero;      // 0x24
    float        m_finetune;             // 0x28
    int          m_loopcount;            // 0x2C
    unsigned int m_flags;                // 0x30

    EventSoundDef();
    FMOD_RESULT init();
};

// size 0x8C
class EventSound: public LinkedListNode {
public:
    EventSoundDef  *m_def;              // 0x0C
    EventLayer     *m_layer;            // 0x10
    EventSound     *m_previous_sound;   // 0x14
    EventI         *m_eventi;           // 0x18, the event that plays it
    unsigned int    m_flags;            // 0x1C
    Sound          *m_sound;            // 0x20
    bool            m_notreadysound;    // 0x24
    DSP            *m_dsp;              // 0x28
    Channel        *m_channel;          // 0x2C
    Channel        *m_last_channel;     // 0x30
    int             m_nextentryindex;   // 0x34
    int             m_lastentryindex;   // 0x38
    int             m_entryindex;       // 0x3C
    int            *m_shuffletable;     // 0x40
    int             m_shuffleindex;     // 0x44
    int             m_shufflesize;      // 0x48
    float           m_spawn_timer;      // 0x4C
    int             m_spawn_current;    // 0x50
    EventSound     *m_parentsound;      // 0x54
    float           m_sounddefvolume;   // 0x58
    float           m_sounddefpitch;    // 0x5C
    FMOD_VECTOR     m_sounddefoffset;   // 0x60
    float           m_last_volume;      // 0x6C
    float           m_last_freq;        // 0x70
    float           m_last_pan;         // 0x74
    LinkedListNode  m_oneshothead;      // 0x78, the sounds a oneshot spawned
    int             m_stream_time;      // 0x84
    unsigned int    m_rampdowntime;     // 0x88

    EventSound();
    FMOD_RESULT init( EventSoundDef *def );
    // Allocates the sounds a oneshot definition can spawn, from mempool.
    FMOD_RESULT createOneshotSounds( SimpleMemPool *mempool );

    // ---- added for fmod_eventimpl_simple (signatures from the retail names; parameter names invented) ----
    FMOD_RESULT release( bool freesound );
    FMOD_RESULT loadFromFEV( File *file, unsigned int fileversion );
    FMOD_RESULT unload();
    FMOD_RESULT stopSound( bool immediate );
    bool isWaitPrevious();
    bool isSoundPlaying();
    bool willTerminate() const;
    FMOD_RESULT getLatestEndTime( unsigned long long *endtime, EventSound **sound );
    FMOD_RESULT isCurrentlyLooping( bool *looping );   // added for fmod_eventlayer
    FMOD_RESULT rescheduleAll();
    FMOD_RESULT setReschedulePending();
    FMOD_RESULT clearPreviousPointers( EventSound *sound );
    FMOD_RESULT setMode( unsigned int mode );
    FMOD_RESULT setMinMaxDistance( float mindistance, float maxdistance );
    FMOD_RESULT setReverbWetLevelDB( float wetlevel );
    FMOD_RESULT setReverbDryLevelDB( float drylevel );
    FMOD_RESULT set3DAttributes( const FMOD_VECTOR *position, const FMOD_VECTOR *velocity,
                                 const FMOD_VECTOR *orientation );
};

} // namespace FMOD

#endif /* _FMOD_EVENTSOUND_H */
