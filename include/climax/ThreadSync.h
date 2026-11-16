#ifndef CLIMAX_THREADSYNC_H
#define CLIMAX_THREADSYNC_H

// Names and layout: Gormiti DWARF (ThreadSync::cLockObject, SpiritFrameWork/Utils/ThreadSync.h:75).
// Only the layout of cLockObject is declared here; its members are not recovered yet.

#include <revolution/os/OSSemaphore.h>
#include <revolution/os/OSThread.h>

namespace ThreadSync {

class cLockObject {
public:
                    cLockObject();

private:
    OSSemaphore     m_Sema;     // 0x00
};

// Gormiti: ThreadSync.h:75; retail inlines it (System.obj has no copy).
inline cLockObject::cLockObject() {
    OSInitSemaphore( &m_Sema, 1 );
}

// The thread ThreadSync::SetMainThread recorded (Gormiti: ThreadSync.cpp:121).
extern OSThread*    gMainThreadID;

// Gormiti: ThreadSync.h:147.
inline bool IsMainThread() {
    return gMainThreadID == OSGetCurrentThread();
}

}

#endif
