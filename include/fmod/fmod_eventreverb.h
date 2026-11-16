/*
 * FMOD::ReverbDef and FMOD::EventReverbI: an event project's reverb definition and the reverb the
 * application sees. Virtuals: retail vtable __vt__Q24FMOD12EventReverbI + Gormiti link map.
 * Layout: Gormiti DWARF (sibling.exe types ReverbDef, EventReverbI).
 */
#ifndef _FMOD_EVENTREVERB_H
#define _FMOD_EVENTREVERB_H

#include <fmod/fmod.h>
#include <fmod/fmod_event.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class EventProjectI;
class EventSystemI;
class Reverb;

// A reverb definition of an event project; linked into the project's list of definitions.
class ReverbDef : public LinkedListNode {
public:
    char                  *m_name;          // 0x0C
    FMOD_REVERB_PROPERTIES m_props;         // 0x10
    EventProjectI         *m_eventproject;  // 0x8C

    ReverbDef();
    FMOD_RESULT release();
};

// size 0xA0
class EventReverbI : public EventReverb, public LinkedListNode {
public:
    ReverbDef             *m_parent;        // 0x10
    FMOD_REVERB_PROPERTIES m_props;         // 0x14
    unsigned int           m_flags;         // 0x90
    EventSystemI          *m_eventsystemi;  // 0x94
    Reverb                *m_reverb;        // 0x98
    void                  *m_userdata;      // 0x9C

    EventReverbI();
    FMOD_RESULT init( Reverb *reverb );

    virtual FMOD_RESULT release();                  // slot 0x08, overrides EventReverb
    virtual FMOD_RESULT set3DAttributes( const FMOD_VECTOR *position, float mindistance, float maxdistance );
    virtual FMOD_RESULT get3DAttributes( FMOD_VECTOR *position, float *mindistance, float *maxdistance );
    virtual FMOD_RESULT setProperties( const FMOD_REVERB_PROPERTIES *props );
    virtual FMOD_RESULT getProperties( FMOD_REVERB_PROPERTIES *props );
    virtual FMOD_RESULT setActive( bool active );
    virtual FMOD_RESULT getActive( bool *active );
    virtual FMOD_RESULT setUserData( void *userdata );
    virtual FMOD_RESULT getUserData( void **userdata );
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused,
                                       unsigned int *memoryused_details );
    virtual ~EventReverbI() {}                      // slot 0x30
};

} // namespace FMOD

#endif /* _FMOD_EVENTREVERB_H */
