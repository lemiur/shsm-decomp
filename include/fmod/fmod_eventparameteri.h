/*
 * FMOD::EventParameterI: a parameter of an event instance (the value an event's envelopes and
 * conditions read). Polymorphic: the 12 slots are the public EventParameter interface.
 * Layout and virtuals: Gormiti DWARF (sibling.exe types EventParameterI), size 0x3C, and
 * __vt__Q24FMOD15EventParameterI (retail lbl_80451C68). Non-virtual methods are declared as units use them.
 */
#ifndef _FMOD_EVENTPARAMETERI_H
#define _FMOD_EVENTPARAMETERI_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class EventEnvelope;
class EventI;
class EventParameterDef;

class EventParameterI : public LinkedListNode {
public:
    EventParameterDef *m_def;           // 0x0C
    float              m_value;         // 0x10
    float              m_seektarget;    // 0x14
    float              m_oldvalue;      // 0x18
    int                m_keyoff;        // 0x1C
    unsigned int       m_flags;         // 0x20
    float              m_rangemin;      // 0x24
    float              m_rangemax;      // 0x28
    EventEnvelope    **m_envelope;      // 0x2C
    EventI            *m_eventi;        // 0x30
    void              *m_userdata;      // 0x34

    // vtable pointer at 0x38: the first virtual below
    virtual FMOD_RESULT getInfo( int *index, char **name );                                 // slot 0x08
    virtual FMOD_RESULT getRange( float *rangemin, float *rangemax );                       // slot 0x0C
    virtual FMOD_RESULT setValue( float value );                                            // slot 0x10
    virtual FMOD_RESULT getValue( float *value );                                           // slot 0x14
    virtual FMOD_RESULT setVelocity( float velocity );                                      // slot 0x18
    virtual FMOD_RESULT getVelocity( float *velocity );                                     // slot 0x1C
    virtual FMOD_RESULT setSeekSpeed( float seekspeed );                                    // slot 0x20
    virtual FMOD_RESULT getSeekSpeed( float *seekspeed );                                   // slot 0x24
    virtual FMOD_RESULT setUserData( void *userdata );                                      // slot 0x28
    virtual FMOD_RESULT getUserData( void **userdata );                                     // slot 0x2C
    virtual FMOD_RESULT keyOff();                                                           // slot 0x30
    virtual FMOD_RESULT getMemoryInfo( unsigned int memorybits, unsigned int event_memorybits,
                                       unsigned int *memoryused, unsigned int *memoryused_array ); // slot 0x34
};

} // namespace FMOD

#endif /* _FMOD_EVENTPARAMETERI_H */
