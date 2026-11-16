/*
 * FMOD::EventCategoryI: a category of events, implementing EventCategory.
 * Layout: Gormiti DWARF (sibling.exe types EventCategoryI), size 0x4C. Virtuals: the retail vtable
 * __vt__Q24FMOD14EventCategoryI (EventCategory's slots).
 */
#ifndef _FMOD_EVENTCATEGORYI_H
#define _FMOD_EVENTCATEGORYI_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class ChannelGroup;
class EventI;
class File;

// Built-in category names (names invented; emitted in .sdata2 by fmod_eventcategoryi and fmod_eventsystemi).
static const char EVENT_CATEGORY_MASTER[] = "master";
static const char EVENT_CATEGORY_MUSIC[]  = "music";

// Max playbacks behaviour flags (m_flags & 7; names invented).
#define EVENTCATEGORYI_MAXPLAYBACKS_MASK                0x00000007
#define EVENTCATEGORYI_MAXPLAYBACKS_STEALNEWEST         0x00000001
#define EVENTCATEGORYI_MAXPLAYBACKS_STEALQUIETEST       0x00000002
#define EVENTCATEGORYI_MAXPLAYBACKS_JUSTFAIL            0x00000003
#define EVENTCATEGORYI_MAXPLAYBACKS_JUSTFAILIFQUIETEST  0x00000004

class EventCategoryI : public EventCategory, public LinkedListNode {
public:
    char           *m_name;             // 0x10
    int             m_index;            // 0x14
    bool            m_paused;           // 0x18
    bool            m_mute;             // 0x19
    float           m_volume;           // 0x1C
    float           m_pitch;            // 0x20
    unsigned int    m_maxplaybacks;     // 0x24
    unsigned short  m_playcount;        // 0x28
    unsigned int    m_flags;            // 0x2C
    LinkedListNode  m_eventhead;        // 0x30, the category's events (EventData_Base::m_category_node)
    ChannelGroup   *m_channelgroup;     // 0x3C
    EventCategoryI *m_categoryhead;     // 0x40
    EventCategoryI *m_parentcategory;   // 0x44
    void           *m_userdata;         // 0x48

    EventCategoryI();

    FMOD_RESULT init();
    FMOD_RESULT release();
    FMOD_RESULT load( File *file, unsigned int fileversion, FMOD_EVENT_LOADINFO *loadinfo,
                      EventCategoryI *parentcategory, const char *categorypath );
    FMOD_RESULT mergeCategory( EventCategoryI *category, bool override );
    FMOD_RESULT createDSPNetwork( EventCategoryI *parentcategory );
    FMOD_RESULT releaseDSPNetwork();
    FMOD_RESULT addCategory( EventCategoryI *category );
    FMOD_RESULT stealEvent( EventI *event );
    inline EventCategoryI *getCategoryPtr( const char *name );

    virtual FMOD_RESULT getInfo( int *index, char **name );
    virtual FMOD_RESULT getCategory( const char *name, EventCategory **category );
    virtual FMOD_RESULT getCategoryByIndex( int index, EventCategory **category );
    virtual FMOD_RESULT getNumCategories( int *numcategories );
    virtual FMOD_RESULT getEventByIndex( int index, unsigned int mode, Event **event );
    virtual FMOD_RESULT getNumEvents( int *numevents );
    virtual FMOD_RESULT stopAllEvents();
    virtual FMOD_RESULT setVolume( float volume );
    virtual FMOD_RESULT getVolume( float *volume );
    virtual FMOD_RESULT setPitch( float pitch, FMOD_EVENT_PITCHUNITS units );
    virtual FMOD_RESULT getPitch( float *pitch, FMOD_EVENT_PITCHUNITS units );
    virtual FMOD_RESULT setPaused( bool paused );
    virtual FMOD_RESULT getPaused( bool *paused );
    virtual FMOD_RESULT setMute( bool mute );
    virtual FMOD_RESULT getMute( bool *mute );
    virtual FMOD_RESULT getChannelGroup( ChannelGroup **channelgroup );
    virtual FMOD_RESULT setUserData( void *userdata );
    virtual FMOD_RESULT getUserData( void **userdata );
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );
};

} // namespace FMOD

#endif /* _FMOD_EVENTCATEGORYI_H */
