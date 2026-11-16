/*
 * FMOD::OutputPolled, the base of the outputs whose hardware buffer is polled: a thread (or the system's
 * update) reads the ring buffer's play position and mixes into the blocks the device has played.
 * Virtuals: retail vtable lbl_8044F7B0 (the Thread part at 0xF0 has its own table, with thunks)
 * Layout: Gormiti DWARF (sibling.exe types OutputPolled), size 0x220
 */
#ifndef _FMOD_OUTPUT_POLLED_H
#define _FMOD_OUTPUT_POLLED_H

#include <fmod/fmod.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_output.h>
#include <fmod/fmod_thread.h>

namespace FMOD {

class OutputPolled : public Output, public Thread {
public:
    int                mCursorBlock;            // 0x214, the next block of the ring buffer to mix
    bool               mPolledFromMainThread;   // 0x218, the mixer runs from System::update
    FMOD_OS_SEMAPHORE *mFinishedSema;           // 0x21C, signalled when a pass has finished

    OutputPolled();                             // 0x8037274C

    FMOD_RESULT start();                        // 0x80372DC4
    FMOD_RESULT stop();                         // 0x80372F4C

    // Override of Thread (slot 0x20 of the vtable). Mixes the blocks the device has played.
    virtual FMOD_RESULT threadFunc();           // 0x803727A0
    virtual ~OutputPolled() {}                  // 0x80372FA8, weak
};

} // namespace FMOD

#endif /* _FMOD_OUTPUT_POLLED_H */
