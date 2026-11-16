/*
 * FMOD::EventLayer: one layer of a complex event: the sounds it plays, the envelopes that drive its
 * DSP units, and the channel group they play through. Not polymorphic; a list node of its event.
 * Layout and method names: Gormiti DWARF (sibling.exe types EventLayer), size 0x4C.
 * Flag names are invented.
 */
#ifndef _FMOD_EVENTLAYER_H
#define _FMOD_EVENTLAYER_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>

namespace FMOD {

class ChannelGroup;
class EventEnvelope;
class EventEnvelopeDef;
class EventI;
class EventParameterI;
class EventSound;

// EventLayer::m_flags bits (names invented from use; unknown bits keep their value).
#define EVENTLAYER_FLAG_0x02    0x0002  // set while the layer plays: rescheduleSounds and update do nothing without it
#define EVENTLAYER_FLAG_0x08    0x0008  // createDSPNetwork and startEffects do nothing with it
#define EVENTLAYER_FLAG_0x10    0x0010  // createDSPNetwork builds the layer's own channel group only with it

class EventLayer : public LinkedListNode {
public:
    EventI           *m_eventi;             // 0x0C
    unsigned int      m_flags;              // 0x10
    int               m_priority;           // 0x14
    int               m_playcount;          // 0x18
    EventParameterI  *m_controlparameter;   // 0x1C
    ChannelGroup     *m_channelgroup;       // 0x20
    ChannelGroup     *m_channelgroup_mem;   // 0x24
    EventSound       *m_pendingsound;       // 0x28
    LinkedListNode    m_soundhead;          // 0x2C, the layer's sounds
    LinkedListNode    m_envelopehead;       // 0x38, the layer's envelopes
    EventEnvelope    *m_envelopememory;     // 0x44
    EventEnvelopeDef *m_envelopedefmemory;  // 0x48

    EventLayer();
    FMOD_RESULT init();
    FMOD_RESULT release();
    FMOD_RESULT dereferenceParentEnvelopes();
    FMOD_RESULT update();
    FMOD_RESULT getLatestEndTime( unsigned long long *end_time, EventSound **sound );
    FMOD_RESULT rescheduleSounds();
    FMOD_RESULT clearPreviousPointers( EventSound *sound );
    bool otherSoundsPlaying( EventSound *sound );
    bool willTerminate() const;
    EventEnvelope *getEnvelope( unsigned int flags );
    FMOD_RESULT createDSPNetwork( ChannelGroup *parentchannelgroup );
    FMOD_RESULT releaseDSPNetwork();
    FMOD_RESULT startEffects( bool start );
};

} // namespace FMOD

#endif /* _FMOD_EVENTLAYER_H */
