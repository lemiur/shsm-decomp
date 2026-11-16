/*
 * FMOD::EventI: the event object behind an FMOD::Event handle, and the data blocks it is made of
 * (EventData_*). An event loaded from a project is a parent: m_data_parent lists its instances; an
 * instance points back to its parent through m_data_instance->m_original.
 * Virtuals: retail vtable __vt__Q24FMOD6EventI + Gormiti link map (38 slots, no destructor).
 * Layout: Gormiti DWARF (sibling.exe types EventI, EventData_Base, EventData_Parent,
 * EventData_Instance, EventData_3D, EventData_2D); retail's constructor and field
 * accesses agree (size 0x118, vptr at 0x114).
 */
#ifndef _FMOD_EVENTI_H
#define _FMOD_EVENTI_H

#include <fmod/fmod.h>
#include <fmod/fmod_channeli.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class ChannelGroup;
class EventCategoryI;
class EventGroupI;
class EventImpl;
class EventParameterI;
class EventSound;
class EventI;
class SimpleMemPool;
class SoundBank;

// EventData_Base::m_exflags bits (names invented from use).
#define EVENTI_EXFLAG_POOLCOPY       0x0040  // set with INUSE when EventInstancePool copies a parent into an instance
#define EVENTI_EXFLAG_INUSE          0x0080  // a pool instance handed out; cleared when it is released

// EventData_Base::m_flags bits (names invented from use).
#define EVENTI_FLAG_TIMEUPDATE       0x00001000  // the event is updated with the time (update)
#define EVENTI_FLAG_PAUSED           0x00002000  // not advancing
#define EVENTI_FLAG_WILLTERMINATE    0x00080000  // the event ends by itself
// EventData_Base::m_flags bits (names invented from use; added for fmod_eventgroupi.cpp).
#define EVENTI_FLAG_0001             0x0001  // purpose unknown: freeEventData refuses an event with it set (FMOD_ERR_EVENT_INTERNAL)
// Added for fmod_eventimpl_simple.cpp.
#define EVENTI_FLAG_0x8000           0x00008000  // every stream of the event is created FMOD_SOFTWARE (createStreams)
#define EVENTI_FLAG_USERDSP          0x00400000  // created with FMOD_EVENT_USERDSP: the instance has its own DSP network
// The event's max playbacks behaviour, read by EventGroupI::stealEventInstance; stealing the
// oldest instance is the default (names invented after FMOD Designer's behaviours).
#define EVENTI_FLAG_STEAL_NEWEST     0x00000200
#define EVENTI_FLAG_STEAL_QUIETEST   0x00000400
#define EVENTI_FLAG_JUST_FAIL        0x00000800
#define EVENTI_FLAG_JUST_FAIL_IF_QUIETEST 0x00800000
// ---- added for fmod_eventimpl_simple ----
#define EVENTI_FLAG_0x20             0x00000020  // keeps a finished event from stopping itself (EventImplSimple::update)
#define EVENTI_FLAG_0x40000          0x00040000  // set while spawned oneshot sounds play (EventImplSimple::update)
#define EVENTI_FLAG_MAXSTREAMS       0x10000000  // a sound failed for lack of channels: the event stops (name invented)
// ---- added for fmod_eventgroupi (names invented) ----
#define EVENTI_FLAG_STEAL_OLDEST     0x00000100  // the default max playbacks behaviour
#define EVENTI_FLAG_SPEAKERLEVELS    0x00000004  // a 2D event whose speaker levels differ from the default
#define EVENTI_EXFLAG_INSTANCEPOOL   0x0020      // the event's instances come from its project's EventInstancePool
// ---- end fmod_eventgroupi ----

// What every event has, parent or instance. size 0x88
struct EventData_Base {
    unsigned int                  m_usereventhandle;        // 0x00
    LinkedListNode                m_category_node;          // 0x04, in the category's event list
    EventGroupI                  *m_eventgroupi;            // 0x10
    EventParameterI              *m_primary_parameteri;     // 0x14
    float                         m_volume;                 // 0x18
    float                         m_uservolume;             // 0x1C
    float                         m_pitch;                  // 0x20
    int                           m_priority;               // 0x24
    unsigned int                  m_mode;                   // 0x28
    FMOD_REVERB_CHANNELPROPERTIES *m_reverbproperties;      // 0x2C
    FMOD_UINT64P                  m_initialdspclock;        // 0x30
    FMOD_UINT64P                  m_lastdspclock;           // 0x38
    FMOD_UINT64P                  m_dspclockelapsed;        // 0x40
    unsigned int                  m_flags;                  // 0x48
    short                         m_exflags;                // 0x4C, EVENTI_EXFLAG_*
    unsigned int                  m_soundbankloadedflags;   // 0x50
    FMOD_RESULT                   m_asyncresult;            // 0x54
    EventCategoryI               *m_categoryi;              // 0x58
    FMOD_EVENT_CALLBACK           m_eventcallback;          // 0x5C
    void                         *m_eventcallbackdata;      // 0x60
    void                         *m_userdata;               // 0x64
    float                         m_pitch_randomization;    // 0x68
    float                         m_volume_randomization;   // 0x6C
    int                           m_stealpriority;          // 0x70
    float                         m_reverbdrylevel_db;      // 0x74
    float                         m_reverbwetlevel_db;      // 0x78
    float                         m_timeoffset;             // 0x7C
    short                         m_fadein;                 // 0x80
    short                         m_fadeout;                // 0x82
    unsigned int                  field_0x84;               // 0x84, not in Gormiti's DWARF (its tail)
};

// What only a parent event has: its instances and sound banks. size 0x44
struct EventData_Parent {
    int           m_numinstances;           // 0x00
    EventI      **m_instance;               // 0x04
    int           m_lastinstanceindex;      // 0x08
    unsigned int  m_systemid;               // 0x0C
    unsigned int  m_projectid;              // 0x10
    FMOD_GUID    *m_guid;                   // 0x14
    char         *m_name;                   // 0x18
    int           m_numsoundbanks;          // 0x1C
    SoundBank   **m_soundbank;              // 0x20
    char        **m_soundbankname;          // 0x24
    int          *m_numsoundbankrefs;       // 0x28
    int         **m_bankentry_array;        // 0x2C
    unsigned int  m_soundbankloadingflags;  // 0x30
    int           m_bankinfosize;           // 0x34
    int           m_waveformcount;          // 0x38
    char        **m_waveformarray;          // 0x3C
    int           m_mempoolsize;            // 0x40
};

// What only an instance has. size 0x24
struct EventData_Instance {
    EventI         *m_original;                 // 0x00, the parent event
    LinkedListNode  m_updatetimeparameter_node; // 0x04
    unsigned int    m_refcnt;                   // 0x10
    unsigned int    m_last_update_time;         // 0x14
    ChannelGroup   *m_channelgroup;             // 0x18
    short           m_fadetime;                 // 0x1C
    SimpleMemPool  *m_mempool;                  // 0x20, the pool the instance's sounds come from
};

// 3D position and distance model. size 0x50
struct EventData_3D {
    FMOD_VECTOR  m_pos;             // 0x00
    FMOD_VECTOR  m_vel;             // 0x0C
    FMOD_VECTOR  m_orientation;     // 0x18
    float        m_mindistance;     // 0x24
    float        m_maxdistance;     // 0x28
    FMOD_VECTOR  m_pos0;            // 0x2C
    FMOD_VECTOR  m_rndpos;          // 0x38
    unsigned int m_rndpos_radius;   // 0x44
    float        m_speakerspread;   // 0x48
    float        m_3dpanlevel;      // 0x4C
};

// 2D speaker levels. size 0x20
struct EventData_2D {
    float m_speakerlevels[8];       // 0x00
};

// size 0x118
class EventI : public LinkedListNode, public Event {
public:
    // Event is an empty handle class: it takes 0x0C-0x10.
    EventData_Base      m_data_base;        // 0x10
    EventData_Parent   *m_data_parent;      // 0x98, NULL in an instance
    EventData_Instance *m_data_instance;    // 0x9C, NULL in a parent
    EventData_3D        m_data_3d;          // 0xA0
    EventData_2D        m_data_2d;          // 0xF0
    EventImpl          *m_impl;             // 0x110, simple or complex body

    EventI();

    static FMOD_RESULT validate( Event *event, EventI **eventi );

    FMOD_RESULT init();
    FMOD_RESULT release( bool freeeventdata, bool releaseinstances );
    FMOD_RESULT createDSPNetwork();
    FMOD_RESULT releaseDSPNetwork();
    FMOD_RESULT copyProperties( EventI *dstevent );
    FMOD_RESULT releaseStreams( bool waituntilready );
    void incRefcnt( bool reset );
    FMOD_RESULT getNumInstancesInUse();
    bool anyBanksLoaded();                   // any of the event's sound banks is loaded (name from Gormiti)
    bool getBankLoaded( int bankindex );
    void setBankLoaded( int bankindex, bool loaded );
    bool useInstancePool();                  // the event comes from its project's EventInstancePool (name invented)

    // ---- added for fmod_eventimpl_simple (signatures from the retail names; parameter names invented) ----
    FMOD_RESULT update();
    bool isSampleAccurate() const;
    FMOD_RESULT setSampleAccurate( bool accurate );
    FMOD_RESULT callEventCallback( FMOD_EVENT_CALLBACKTYPE type, void *param1, void *param2 );
    FMOD_RESULT playSound( EventSound *sound, int delay, bool *played );

    // The parent event of an instance; NULL for a parent (accessor name invented).
    EventI *getOriginal() const { return m_data_instance ? m_data_instance->m_original : NULL; }

    // The pool an instance's sounds come from; NULL for a parent or an instance outside the pool (name invented).
    SimpleMemPool *getMemPool() const { return m_data_instance ? m_data_instance->m_mempool : NULL; }
    // The instance's channel group; NULL for a parent (name invented).
    ChannelGroup *getInstanceChannelGroup() const { return m_data_instance ? m_data_instance->m_channelgroup : NULL; }

    // ---- added for fmod_eventimpl_simple: property accessors (names invented). EventImplSimple's setters read
    // the event's properties through inline calls: retail evaluates them before the sound pointer and the
    // arguments right to left, which only an inlined call per argument reproduces.
    unsigned int getMode() const { return m_data_base.m_mode; }
    float getMinDistance() const { return m_data_3d.m_mindistance; }
    float getMaxDistance() const { return m_data_3d.m_maxdistance; }
    float getReverbWetLevelDB() const { return m_data_base.m_reverbwetlevel_db; }
    float getReverbDryLevelDB() const { return m_data_base.m_reverbdrylevel_db; }
    // The clocks are returned by value: getPositionMs keeps the two copies each makes.
    FMOD_UINT64P getDSPClockElapsed() const { return m_data_base.m_dspclockelapsed; }
    FMOD_UINT64P getLastDSPClock() const { return m_data_base.m_lastdspclock; }
    const FMOD_VECTOR *getVelocity() const { return &m_data_3d.m_vel; }
    const FMOD_VECTOR *getOrientation() const { return &m_data_3d.m_orientation; }
    // The pool size counted so far: the parent's, for an instance.
    int getMemPoolSize() const { return getOriginal() ? getOriginal()->m_data_parent->m_mempoolsize : m_data_parent->m_mempoolsize; }
    // ---- added for fmod_eventgroupi ----
    FMOD_RESULT setupInstance();
    FMOD_RESULT estimateAudibility( float *audibility );
    FMOD_RESULT createStreams( unsigned int eventmode );
    bool allBanksLoaded();                   // every sound bank of the event is loaded (name from Gormiti)
    void setBankLoading( int bankindex, bool loading );

    // The event's EVENTI_FLAG_* bits (accessor names invented).
    unsigned int getFlags() const { return m_data_base.m_flags; }
    void setFlags( unsigned int flags ) { m_data_base.m_flags = flags; }

    // The DSP clock at which the event started, returned by value (name invented).
    FMOD_UINT64P getStartClock() const { return m_data_base.m_initialdspclock; }

    // The event's name, which an instance takes from its parent (name invented).
    char *getName() const {
        return getOriginal() ? getOriginal()->m_data_parent->m_name : ( m_data_parent ? m_data_parent->m_name : NULL );
    }

    FMOD_RESULT setNumInstances( int numinstances );
    // ---- end fmod_eventgroupi ----

    // vtable pointer at 0x114: the first virtual below
    virtual FMOD_RESULT start();                                                        // slot 0x08
    virtual FMOD_RESULT stop( bool immediate );
    virtual FMOD_RESULT getInfo( int *index, char **name, FMOD_EVENT_INFO *info );
    virtual FMOD_RESULT getParameter( const char *name, EventParameter **parameter );
    virtual FMOD_RESULT getParameterByIndex( int index, EventParameter **parameter );
    virtual FMOD_RESULT getNumParameters( int *numparameters );
    virtual FMOD_RESULT getProperty( const char *propertyname, void *value, bool this_instance );
    virtual FMOD_RESULT getPropertyByIndex( int propertyindex, void *value, bool this_instance );
    virtual FMOD_RESULT setProperty( const char *propertyname, void *value, bool this_instance );
    virtual FMOD_RESULT setPropertyByIndex( int propertyindex, void *value, bool this_instance );
    virtual FMOD_RESULT getNumProperties( int *numproperties );
    virtual FMOD_RESULT getPropertyInfo( int *propertyindex, char **propertyname, FMOD_EVENTPROPERTY_TYPE *type );
    virtual FMOD_RESULT getCategory( EventCategory **category );
    virtual FMOD_RESULT setVolume( float volume );                                      // slot 0x3C
    virtual FMOD_RESULT getVolume( float *volume );
    virtual FMOD_RESULT setVolumeRandomization( float randomization );
    virtual FMOD_RESULT getVolumeRandomization( float *randomization );
    virtual FMOD_RESULT setPitch( float pitch, FMOD_EVENT_PITCHUNITS units );
    virtual FMOD_RESULT getPitch( float *pitch, FMOD_EVENT_PITCHUNITS units );
    virtual FMOD_RESULT setPitchRandomization( float randomization );
    virtual FMOD_RESULT getPitchRandomization( float *randomization );
    virtual FMOD_RESULT setPaused( bool paused );
    virtual FMOD_RESULT getPaused( bool *paused );
    virtual FMOD_RESULT setMute( bool mute );
    virtual FMOD_RESULT getMute( bool *mute );
    virtual FMOD_RESULT set3DAttributes( const FMOD_VECTOR *position, const FMOD_VECTOR *velocity,
                                         const FMOD_VECTOR *orientation );
    virtual FMOD_RESULT get3DAttributes( FMOD_VECTOR *position, FMOD_VECTOR *velocity, FMOD_VECTOR *orientation );
    virtual FMOD_RESULT set3DOcclusion( float directocclusion, float reverbocclusion );
    virtual FMOD_RESULT get3DOcclusion( float *directocclusion, float *reverbocclusion );
    virtual FMOD_RESULT setReverbProperties( const FMOD_REVERB_CHANNELPROPERTIES *prop );
    virtual FMOD_RESULT getReverbProperties( FMOD_REVERB_CHANNELPROPERTIES *prop );
    virtual FMOD_RESULT getState( unsigned int *state );
    virtual FMOD_RESULT setCallback( FMOD_EVENT_CALLBACK callback, void *userdata );
    virtual FMOD_RESULT getParentGroup( EventGroup **group );
    virtual FMOD_RESULT getChannelGroup( ChannelGroup **channelgroup );
    virtual FMOD_RESULT setUserData( void *userdata );
    virtual FMOD_RESULT getUserData( void **userdata );
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );                // slot 0x9C
};

} // namespace FMOD

#endif /* _FMOD_EVENTI_H */
