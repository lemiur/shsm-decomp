/*
 * FMOD::AsyncThread: a worker thread that services FMOD_NONBLOCKING sound opens and runs registered
 * callbacks (fmod_async.cpp). Class name invented (FMOD convention); member names marked "placeholder"
 * are not known.
 */
#ifndef _FMOD_ASYNC_H
#define _FMOD_ASYNC_H

#include "fmod.h"
#include "fmod_linkedlist.h"
#include "fmod_os_misc.h"
#include "fmod_thread.h"

namespace FMOD {

class SoundI;
class SystemI;
class AsyncThread;

// A callback run by the async thread on each pass.
typedef FMOD_RESULT (*AsyncCallback)();

// The parameters of a non-blocking sound open, kept in SoundI::mAsyncData until the async thread
// has opened the sound (layout and names: Gormiti DWARF, size 0x294).
class AsyncData {
public:
    char                    mName[512];         // 0x0, the file name
    unsigned int            mBufferSize;        // 0x200
    unsigned int            mBufferSizeType;    // 0x204
    AsyncThread            *mThread;            // 0x208, the thread opening the sound
    LinkedListNode          mNode;              // 0x20C, link in AsyncThread::mHead
    void                   *mNameData;          // 0x218, the data of an FMOD_OPENMEMORY open
    FMOD_CREATESOUNDEXINFO  mExInfo;            // 0x21C
    bool                    mExInfoExists;      // 0x28C
    FMOD_RESULT             mResult;            // 0x290, the result of the open
};

class AsyncThread : public LinkedListNode {
public:
    Thread                   mThread;           // 0xC
    bool                     mThreadActive;     // 0x130
    LinkedListNode           mHead;             // 0x134, sounds waiting to be opened (node data: SoundI*)
    FMOD_OS_CRITICALSECTION *mCrit;             // 0x140, guards mHead and mCallbackHead
    bool                     mOwned;            // 0x144, released by update once its sound is open
    bool                     mBusy;             // 0x145, a sound is being opened
    bool                     mDone;             // 0x146, released by update
    LinkedListNode           mCallbackHead;     // 0x148, callbacks (node data: AsyncCallback)

    static LinkedListNode    gAsyncHead;        // the list of async threads

    AsyncThread();
    FMOD_RESULT        init( bool owned, SystemI *system );   // starts the thread
    FMOD_RESULT        release();               // no copy in retail (dead-stripped)

    static FMOD_RESULT shutDown();           // releases every async thread
    static FMOD_RESULT addCallback( AsyncCallback callback, AsyncThread **thread );   // adds a callback
    static FMOD_RESULT removeCallback( AsyncCallback callback );                         // removes a callback
    FMOD_RESULT        reallyRelease();           // releases this thread
    FMOD_RESULT        wakeupThread();
    static FMOD_RESULT update();           // releases the threads that are done
    FMOD_RESULT        threadFunc();           // the thread's body
    static FMOD_RESULT getAsyncThread( SoundI *sound );   // finds or creates an async thread
};

} // namespace FMOD

#endif /* _FMOD_ASYNC_H */
