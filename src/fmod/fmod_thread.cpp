/*
 * fmod_thread.cpp: FMOD::Thread, the operating-system thread that drives an FMOD object.
 * Class, member and threadFunc names come from Gormiti's DWARF (sibling.exe types and pair).
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_thread.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_string.h>
#include <fmod/fmod_thread.h>

namespace FMOD {

// Thread table of the system pool: slot 0 is not searched. The two helpers below stand for code
// retail has inlined into Thread::callback (names invented): the by-value id of the first, copied to
// a stack slot because its address is taken, is what the bytes show.

/*
====================
RegisterThreadSlot

Enters the thread in the pool's thread table, in the first free slot, unless it is in the table already;
asks the operating system for the id when the caller has none.
====================
*/
static inline void RegisterThreadSlot( unsigned int threadid, MemPool *pool ) {
    if ( threadid == 0 ) {
        FMOD_OS_Thread_GetCurrentID( &threadid );
    }
    for ( int slot = 1; slot < MemPool::MAX_THREADS; slot++ ) {
        if ( pool->mCurrentAllocatedThreadID[slot] == threadid ) {
            break;
        }
        if ( pool->mCurrentAllocatedThreadID[slot] == 0 ) {
            pool->mCurrentAllocatedThreadID[slot] = threadid;
            pool->mCurrentAllocated[slot]         = 0;
            break;
        }
    }
}

/*
====================
UnregisterThreadSlot

Clears the thread's slot in the pool's thread table, counter included.
====================
*/
static inline void UnregisterThreadSlot( unsigned int threadid, MemPool *pool ) {
    for ( int slot = 1; slot < MemPool::MAX_THREADS; slot++ ) {
        if ( pool->mCurrentAllocatedThreadID[slot] == threadid ) {
            pool->mCurrentAllocated[slot]         = 0;
            pool->mCurrentAllocatedThreadID[slot] = 0;
            break;
        }
    }
}

/*
====================
Thread::callback

Entry point of the operating-system thread (the Thread is the argument). Enters the thread in the
system pool's thread table, then runs the user callback (or threadFunc) once per pass until
closeThread clears mRunning, and leaves the table again.
====================
*/
unsigned int Thread::callback( void *param ) {
    unsigned int id;
    Thread *thread = (Thread *)param;

    FMOD_OS_Thread_GetCurrentID( &id );
    RegisterThreadSlot( id, lbl_80529020->mMemPool );

    thread->mRunning = true;

    while ( thread->mRunning ) {
        if ( thread->mSema ) {
            FMOD_OS_Semaphore_Wait( thread->mSema );
        }

        if ( thread->mRunning ) {
            if ( thread->mUserCallback ) {
                thread->mUserCallback( thread->mUserData );
            } else {
                thread->threadFunc();
            }

            if ( thread->mPeriod ) {
                FMOD_OS_Time_Sleep( thread->mPeriod );
            }
        }
    }

    UnregisterThreadSlot( id, lbl_80529020->mMemPool );

    FMOD_OS_Semaphore_Signal( thread->mEndSema, false );

    return 0;
} // 0x80376FC8

/*
====================
Thread::threadFunc

Base thread body (vtable slot 0x08, name from Gormiti's pairing): does nothing and returns FMOD_OK.
====================
*/
FMOD_RESULT Thread::threadFunc() {
    return FMOD_OK;
} // 0x8037713C

/*
====================
Thread::Thread

Clears the members; the vtable pointer is set by the compiler.
====================
*/
Thread::Thread() {
    mHandle       = 0;
    mRunning      = false;
    mStack        = 0;
    mUserData     = 0;
    mSema         = 0;
    mEndSema      = 0;
    mPeriod       = 0;
    mUserCallback = 0;
} // 0x80377144

/*
====================
Thread::initThread

Starts the thread: records the callback, creates the semaphore when asked, translates the priority
to the operating-system layer's, copies the name and creates the thread, then waits until it is
running and reports the new thread to the application's system callback.
====================
*/
FMOD_RESULT Thread::initThread( const char *name, void ( *func )( void * ), void *param, PRIORITY priority,
                                void *stack, int stacksize, bool usesemaphore, int sleepperiod,
                                SystemI *system ) {
    FMOD_RESULT result;
    FMOD_THREAD_PRIORITY pri;

    mUserCallback = func;
    mUserData     = param;
    mPeriod       = sleepperiod;
    mRunning      = false;

    if ( usesemaphore ) {
        result = FMOD_OS_Semaphore_Create( &mSema );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    switch ( priority ) {
    case PRIORITY_VERYLOW:
        pri = FMOD_THREAD_PRIORITY_VERYLOW;
        break;
    case PRIORITY_LOW:
        pri = FMOD_THREAD_PRIORITY_LOW;
        break;
    case PRIORITY_NORMAL:
        pri = FMOD_THREAD_PRIORITY_NORMAL;
        break;
    case PRIORITY_HIGH:
        pri = FMOD_THREAD_PRIORITY_HIGH;
        break;
    case PRIORITY_VERYHIGH:
        pri = FMOD_THREAD_PRIORITY_VERYHIGH;
        break;
    case PRIORITY_CRITICAL:
        pri = FMOD_THREAD_PRIORITY_CRITICAL;
        break;
    default:
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( name ) {
        FMOD_strncpy( mName, name, 256 );
    } else {
        FMOD_strcpy( mName, "?????" );
    }

    result = FMOD_OS_Thread_Create( name, callback, this, pri, stack, stacksize, &mHandle );
    if ( result != FMOD_OK ) {
        return result;
    }

    while ( !mRunning ) {
        FMOD_OS_Time_Sleep( 1 );
    }

    if ( lbl_80529020->mSystemCallback ) {
        lbl_80529020->mSystemCallback( (FMOD_SYSTEM *)system, FMOD_SYSTEM_CALLBACKTYPE_THREADCREATED, mHandle,
                                       (void *)name );
    }

    return FMOD_OK;
} // 0x80377178

/*
====================
Thread::closeThread

Stops a running thread: clears mRunning, wakes it through its semaphore, waits for its end semaphore,
then frees the semaphores, the operating-system thread and the stack.
====================
*/
FMOD_RESULT Thread::closeThread() {
    if ( mRunning ) {
        FMOD_RESULT result;

        result = FMOD_OS_Semaphore_Create( &mEndSema );
        if ( result != FMOD_OK ) {
            return result;
        }

        mRunning = false;

        if ( mSema ) {
            result = FMOD_OS_Semaphore_Signal( mSema, false );
            if ( result != FMOD_OK ) {
                return result;
            }
        }

        result = FMOD_OS_Semaphore_Wait( mEndSema );
        if ( result != FMOD_OK ) {
            return result;
        }

        if ( mSema ) {
            result = FMOD_OS_Semaphore_Free( mSema );
            if ( result != FMOD_OK ) {
                return result;
            }
            mSema = 0;
        }

        result = FMOD_OS_Semaphore_Free( mEndSema );
        if ( result != FMOD_OK ) {
            return result;
        }
        mEndSema = 0;

        result = FMOD_OS_Thread_Destroy( mHandle );
        if ( result != FMOD_OK ) {
            return result;
        }
        mHandle = 0;

        if ( mStack ) {
#line 320
            FMOD_Memory_Free( mStack );
            mStack = 0;
        }
    }

    return FMOD_OK;
} // 0x803772F0

/*
====================
Thread::wakeupThread

Signals the thread's semaphore when it has one, passing the flag on; otherwise returns FMOD_OK.
====================
*/
FMOD_RESULT Thread::wakeupThread( bool param ) {
    if ( mSema ) {
        return FMOD_OS_Semaphore_Signal( mSema, param );
    }
    return FMOD_OK;
} // 0x80377404

} // namespace FMOD
