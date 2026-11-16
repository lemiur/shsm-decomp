/*
 * FMOD::EventInstancePool: a fixed set of prebuilt simple-event instances (EventI with one
 * EventSound each) handed out round robin, stealing from playing events when all are in use.
 * Layout: Gormiti DWARF (sibling.exe types EventInstancePool), size 0x10.
 */
#ifndef _FMOD_EVENTINSTANCEPOOL_H
#define _FMOD_EVENTINSTANCEPOOL_H

#include <fmod/fmod.h>

namespace FMOD {

class EventI;
class SimpleMemPool;

// size 0x10
class EventInstancePool {
public:
    unsigned int   m_numinstances;          // 0x00
    unsigned int   m_lastinstanceindex;     // 0x04, where the round-robin search resumes
    EventI       **m_instance;              // 0x08
    SimpleMemPool *m_instance_mempool;      // 0x0C, the instances' memory

    EventInstancePool();
    FMOD_RESULT init( unsigned int numinstances );
    FMOD_RESULT release();
    FMOD_RESULT buildEventInstance( EventI **event, SimpleMemPool *mempool );
    FMOD_RESULT getEventInstance( EventI *parentevent, EventI **dstevent );
    FMOD_RESULT releaseEventInstance( EventI *event, bool waituntilready );
    FMOD_RESULT copyEventInstance( EventI *srcevent, EventI *dstevent );
    FMOD_RESULT getEventInstanceIndex( EventI *event, int *index );
    FMOD_RESULT getEventInstancePtr( unsigned int index, EventI **event );
    FMOD_RESULT getNumInstancesInUse( EventI *event, int *numinstances );
};

} // namespace FMOD

#endif /* _FMOD_EVENTINSTANCEPOOL_H */
