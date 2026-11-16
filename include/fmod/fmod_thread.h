/*
 * FMOD::Thread: an operating-system thread owned by an FMOD object (fmod_thread.cpp). The class and
 * member names come from Gormiti's DWARF
 * (sibling.exe types FMOD::Thread); the vtable pointer sits at 0x120, after the data members
 * (size 0x124).
 */
#ifndef _FMOD_THREAD_H
#define _FMOD_THREAD_H

#include "fmod.h"
#include "fmod_os_misc.h"

namespace FMOD {

class SystemI;

class Thread {
public:
    // Thread priorities (Gormiti DWARF, fmod_thread.cpp).
    enum PRIORITY {
        PRIORITY_VERYLOW  = -2,
        PRIORITY_LOW      = -1,
        PRIORITY_NORMAL   = 0,
        PRIORITY_HIGH     = 1,
        PRIORITY_VERYHIGH = 2,
        PRIORITY_CRITICAL = 3
    };

    char                        mName[256];         // 0x00
    void                       *mHandle;            // 0x100
    bool                        mRunning;           // 0x104
    void                       *mUserData;          // 0x108, the argument of mUserCallback
    void                       *mStack;             // 0x10C
    FMOD_OS_SEMAPHORE          *mSema;              // 0x110, waited on before each pass
    FMOD_OS_SEMAPHORE          *mEndSema;           // 0x114, signalled when the thread ends
    void                      (*mUserCallback)( void * ); // 0x118, NULL runs threadFunc
    int                         mPeriod;            // 0x11C, sleep between passes (ms)

    Thread();
    // Slot 0x08 of the vtable (name from the pairing with Gormiti's build); the base body returns FMOD_OK.
    virtual FMOD_RESULT threadFunc();
    virtual ~Thread() {}

    // Thread entry point: the object is passed as the argument (Gormiti name and signature).
    static unsigned int callback( void *param );

    // Starts the thread running func( param ); parameter names invented.
    FMOD_RESULT initThread( const char *name, void (*func)( void * ), void *param, PRIORITY priority,
                            void *stack, int stacksize, bool usesemaphore, int sleepperiod,
                            SystemI *system );
    FMOD_RESULT closeThread();              // closes the thread
    FMOD_RESULT wakeupThread( bool param );

    // Declared only: Gormiti lists it as unused, so no body is written here.
    static FMOD_RESULT getCurrentThreadID( unsigned int *id );
};

} // namespace FMOD

#endif /* _FMOD_THREAD_H */
