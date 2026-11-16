/*
 * FMOD::SoundBank: a sound bank (FSB file) of an event project; sample banks are loaded whole,
 * stream banks are read through StreamInstances.
 * Layout: Gormiti DWARF (sibling.exe types SoundBank, StreamInstance, SoundBankQueueEntry), size
 * 0x58 (a plain class with no virtuals).
 * Partial declaration: only the methods fmod_eventgroupi.cpp calls are declared; the unit that
 * defines SoundBank adds the rest.
 */
#ifndef _FMOD_SOUNDBANK_H
#define _FMOD_SOUNDBANK_H

#include <fmod/fmod.h>
#include <fmod/fmod_linkedlist.h>
#include <fmod/fmod_os_misc.h>

namespace FMOD {

class Bundle;
class EventGroupI;
class EventI;
class EventProjectI;
class Sound;
class SoundBank;

// One open stream of a stream bank. size 0x18
class StreamInstance : public LinkedListNode {
public:
    bool          m_used;               // 0x0C
    unsigned int  m_mode;               // 0x10
    Sound        *m_sound;              // 0x14
};

// A non-blocking sound bank load waiting for the loader thread. size 0x2C
class SoundBankQueueEntry : public LinkedListNode {
public:
    SoundBank     *soundbank;           // 0x0C
    bool           is_sample;           // 0x10
    EventI        *event;               // 0x14
    EventGroupI   *eventgroup;          // 0x18
    Bundle        *bundle;              // 0x1C
    unsigned int   mode;                // 0x20
    int           *entrylist;           // 0x24
    int            entrycount;          // 0x28
};

// size 0x58
class SoundBank : public LinkedListNode {
public:
    unsigned int    m_mode;             // 0x0C
    char           *m_name;             // 0x10
    char           *m_fullpathname[2];  // 0x14
    Sound          *m_sound;            // 0x1C
    StreamInstance  m_stream;           // 0x20
    int             m_streamrefcnt;     // 0x38
    int            *m_subsoundrefcnt;   // 0x3C
    int             m_maxstreams;       // 0x40
    char           *m_encryptionkey;    // 0x44
    EventProjectI  *m_project;          // 0x48
    int             m_numsubsounds;     // 0x4C
    unsigned int    field_0x50[2];      // 0x50, m_fsbhash (64-bit)

    // The queue of pending non-blocking loads and the critical section guarding it (Gormiti map).
    static SoundBankQueueEntry      gSoundBankQueueUsedHead;
    static FMOD_OS_CRITICALSECTION *gSoundbankQueueCrit;

    // Waits until the loader thread has emptied the queue.
    static FMOD_RESULT flushLoadQueue();

    bool isSample() const;
    FMOD_RESULT decRefcnt( EventI *eventi );
    FMOD_RESULT releaseSampleInstance( EventI *eventi, bool releasesound );

    // ---- added for fmod_eventimpl_simple (signature from the retail name; parameter names invented) ----
    FMOD_RESULT createStreamInstance( unsigned int mode, unsigned int eventmode, EventI *eventi, EventGroupI *eventgroupi );
    FMOD_RESULT createSampleInstance( unsigned int eventmode, EventI *event, EventGroupI *eventgroup, Bundle *bundle );
};

} // namespace FMOD

#endif /* _FMOD_SOUNDBANK_H */
