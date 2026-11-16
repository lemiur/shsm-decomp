/*
 * fmod_async.cpp: the async threads that open FMOD_NONBLOCKING sounds and run registered callbacks.
 * Unit named from its "fmod_async.cpp" strings (allocation tags). Allocation tags carry the original
 * file's line numbers, restored with #line at each tagged statement (docs/exceptions.md, "FMOD rulings
 * (fmod-1)").
 */

#include <fmod/fmod.h>
#include <fmod/fmod_async.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_soundi.h>
#include <fmod/fmod_systemi.h>
#include <new>

namespace FMOD {

LinkedListNode AsyncThread::gAsyncHead;

/*
====================
asyncThreadFunc

The thread entry: runs the async thread's body.
====================
*/
void asyncThreadFunc( void *data ) {
    ( (AsyncThread *)data )->threadFunc();
} // 0x8032BDAC

/*
====================
AsyncThread::AsyncThread

Inlined into getAsyncThread; its out-of-line copy is dead-stripped (Gormiti map: UNUSED).
====================
*/
AsyncThread::AsyncThread() {
    mCrit         = NULL;
    mThreadActive = false;
    mBusy         = false;
    mDone         = false;
}

/*
====================
AsyncThread::shutDown

Releases every async thread.
====================
*/
FMOD_RESULT AsyncThread::shutDown() {
    if ( lbl_80529020->mAsyncCrit ) {
        LinkedListNode *current;

        FMOD_OS_CriticalSection_Enter( lbl_80529020->mAsyncCrit );

        current = gAsyncHead.getNext();
        while ( current != &gAsyncHead ) {
            LinkedListNode *next = current->getNext();

            static_cast< AsyncThread * >( current )->reallyRelease();
            current = next;
        }

        FMOD_OS_CriticalSection_Leave( lbl_80529020->mAsyncCrit );
    }

    return FMOD_OK;
} // 0x8032BDF0

/*
====================
AsyncThread::addCallback

Adds a callback to the async thread, creating the thread if there is none.
====================
*/
FMOD_RESULT AsyncThread::addCallback( AsyncCallback callback, AsyncThread **thread ) {
    LinkedListNode *node;
    AsyncThread    *asyncthread;
    FMOD_RESULT     result;

    if ( thread ) {
        *thread = NULL;
    }

#line 120
    node = new ( FMOD_Memory_Alloc( sizeof( LinkedListNode ) ) ) LinkedListNode;
    if ( !node ) {
        return FMOD_ERR_MEMORY;
    }
    node->mNodeData = (void *)callback;

    result = getAsyncThread( NULL );
    if ( result != FMOD_OK ) {
        return result;
    }

    asyncthread = static_cast< AsyncThread * >( gAsyncHead.getNext() );
    if ( asyncthread != &gAsyncHead ) {
        FMOD_OS_CriticalSection_Enter( lbl_80529020->mAsyncCrit );
        node->addBefore( &asyncthread->mCallbackHead );
        FMOD_OS_CriticalSection_Leave( lbl_80529020->mAsyncCrit );

        if ( thread ) {
            *thread = asyncthread;
        }
    } else {
        return FMOD_ERR_INTERNAL;
    }

    return FMOD_OK;
} // 0x8032BE64

/*
====================
AsyncThread::removeCallback

Removes a callback from the async thread.
====================
*/
FMOD_RESULT AsyncThread::removeCallback( AsyncCallback callback ) {
    AsyncThread    *asyncthread;
    LinkedListNode *current;

    asyncthread = static_cast< AsyncThread * >( gAsyncHead.getNext() );
    if ( asyncthread != &gAsyncHead ) {
        FMOD_OS_CriticalSection_Enter( lbl_80529020->mAsyncCrit );

        for ( current = asyncthread->mCallbackHead.getNext(); current != &asyncthread->mCallbackHead;
              current = current->getNext() ) {
            if ( current->mNodeData == (void *)callback ) {
                current->removeNode();
#line 190
                FMOD_Memory_Free( current );
                break;
            }
        }

        FMOD_OS_CriticalSection_Leave( lbl_80529020->mAsyncCrit );
    } else {
        return FMOD_ERR_INTERNAL;
    }

    return FMOD_OK;
} // 0x8032BF7C

/*
====================
AsyncThread::init

Creates the thread's lock, starts the thread and links it into the list of async threads.
Inlined into getAsyncThread; its out-of-line copy is dead-stripped (Gormiti map: UNUSED).
====================
*/
FMOD_RESULT AsyncThread::init( bool owned, SystemI *system ) {
    FMOD_RESULT result;

    mOwned = owned;

    result = FMOD_OS_CriticalSection_Create( &mCrit, false );
    if ( result != FMOD_OK ) {
        return result;
    }

    result = mThread.initThread( "FMOD thread for FMOD_NONBLOCKING", asyncThreadFunc, this, Thread::PRIORITY_HIGH,
                                 NULL, 32 * 1024, true, 0, system );
    if ( result != FMOD_OK ) {
        return result;
    }

    mThreadActive = true;

    FMOD_OS_CriticalSection_Enter( lbl_80529020->mAsyncCrit );
    addBefore( &gAsyncHead );
    FMOD_OS_CriticalSection_Leave( lbl_80529020->mAsyncCrit );

    return FMOD_OK;
}

/*
====================
AsyncThread::reallyRelease

Frees the thread's callbacks, closes the thread and frees it.
====================
*/
FMOD_RESULT AsyncThread::reallyRelease() {
    LinkedListNode *current;

    FMOD_OS_CriticalSection_Enter( mCrit );

    current = mCallbackHead.getNext();
    while ( current != &mCallbackHead ) {
        LinkedListNode *next = current->getNext();

        current->removeNode();
#line 309
        FMOD_Memory_Free( current );
        current = next;
    }

    FMOD_OS_CriticalSection_Leave( mCrit );

    removeNode();
    mThreadActive = false;
    mThread.closeThread();

    if ( mCrit ) {
        FMOD_OS_CriticalSection_Free( mCrit, false );
    }

#line 329
    FMOD_Memory_Free( this );
    return FMOD_OK;
} // 0x8032C054

/*
====================
AsyncThread::wakeupThread
====================
*/
FMOD_RESULT AsyncThread::wakeupThread() {
    return mThread.wakeupThread( false );
} // 0x8032C154

/*
====================
AsyncThread::update

Releases the async threads that have finished.
====================
*/
FMOD_RESULT AsyncThread::update() {
    if ( lbl_80529020->mAsyncCrit ) {
        LinkedListNode *current;

        FMOD_OS_CriticalSection_Enter( lbl_80529020->mAsyncCrit );

        current = gAsyncHead.getNext();
        while ( current != &gAsyncHead ) {
            AsyncThread    *asyncthread = static_cast< AsyncThread * >( current );
            LinkedListNode *next        = current->getNext();

            if ( asyncthread->mDone ) {
                asyncthread->reallyRelease();
            }
            current = next;
        }

        FMOD_OS_CriticalSection_Leave( lbl_80529020->mAsyncCrit );
    }

    return FMOD_OK;
} // 0x8032C160

/*
====================
AsyncThread::threadFunc

The thread's body: opens the next sound waiting in mHead (or finishes its seek), reports the
result to the sound, then runs the registered callbacks.
====================
*/
FMOD_RESULT AsyncThread::threadFunc() {
    FMOD_RESULT result = FMOD_OK;
    SoundI     *sound  = NULL;
    SystemI    *system;

    if ( mThreadActive ) {
        FMOD_OS_CriticalSection_Enter( mCrit );
        if ( mHead.getNext() != &mHead ) {
            sound = (SoundI *)mHead.getNext()->mNodeData;
            mHead.getNext()->removeNode();
            mBusy = true;
        }
        FMOD_OS_CriticalSection_Leave( mCrit );

        if ( sound ) {
            system = sound->mSystem;

            if ( sound->mOpenState == FMOD_OPENSTATE_LOADING ) {
                if ( sound->mMode & ( FMOD_OPENMEMORY | FMOD_OPENMEMORY_POINT ) ) {
                    result = system->createSoundInternal(
                        (const char *)sound->mAsyncData->mNameData, sound->mMode, sound->mAsyncData->mBufferSize,
                        sound->mAsyncData->mBufferSizeType,
                        sound->mAsyncData->mExInfoExists ? &sound->mAsyncData->mExInfo : NULL, true, &sound );
                } else {
                    result = system->createSoundInternal(
                        sound->mAsyncData->mName, sound->mMode, sound->mAsyncData->mBufferSize,
                        sound->mAsyncData->mBufferSizeType,
                        sound->mAsyncData->mExInfoExists ? &sound->mAsyncData->mExInfo : NULL, true, &sound );
                }
            } else if ( sound->mOpenState == FMOD_OPENSTATE_SEEKING ) {
                Stream *stream = (Stream *)sound;

                result = stream->updateSubSound( stream->mSubSoundIndex, false );
                if ( result == FMOD_OK ) {
                    result = stream->setPosition( 0, FMOD_TIMEUNIT_PCM );
                    if ( result == FMOD_OK ) {
                        result = stream->flush();
                    }
                }
            }

            sound->mAsyncData->mThread  = NULL;
            sound->mAsyncData->mResult  = result;
            sound->mFlags              |= SOUNDI_FLAG_ASYNCFINISHING;
            sound->mOpenState           = ( result == FMOD_OK ) ? FMOD_OPENSTATE_READY : FMOD_OPENSTATE_ERROR;
            if ( sound->mSubSoundParent ) {
                sound->mSubSoundParent->mOpenState = sound->mOpenState;
            }

            mBusy = false;

            if ( sound->mAsyncData->mExInfoExists && sound->mAsyncData->mExInfo.nonblockcallback ) {
                sound->mUserData = sound->mAsyncData->mExInfo.userdata;
                sound->mAsyncData->mExInfo.nonblockcallback( (FMOD_SOUND *)sound, result );
            }

            sound->mFlags &= ~SOUNDI_FLAG_ASYNCFINISHING;

            if ( mOwned ) {
                mDone = true;
            }
        }

        FMOD_OS_CriticalSection_Enter( mCrit );
        LinkedListNode *node = mCallbackHead.getNext();
        FMOD_OS_CriticalSection_Leave( mCrit );

        while ( node != &mCallbackHead ) {
            result = ( (AsyncCallback)node->mNodeData )();
            if ( result != FMOD_OK ) {
                return result;
            }

            FMOD_OS_CriticalSection_Enter( mCrit );
            node = node->getNext();
            FMOD_OS_CriticalSection_Leave( mCrit );
        }
    }

    return FMOD_OK;
} // 0x8032C1E0

/*
====================
AsyncThread::getAsyncThread

Finds the async thread, creating it on first use, and assigns it to sound (NULL: only makes sure
the thread exists). Gormiti's copy (another revision: it colours found before the list's
first node) has no current local; retail's register order needs it.
====================
*/
FMOD_RESULT AsyncThread::getAsyncThread( SoundI *sound ) {
    FMOD_RESULT     result;
    bool            found       = false;
    AsyncThread    *asyncthread = NULL;
    LinkedListNode *current;

    FMOD_OS_CriticalSection_Enter( lbl_80529020->mAsyncCrit );
    current = gAsyncHead.getNext();
    if ( current != &gAsyncHead ) {
        asyncthread = (AsyncThread *)current;

        FMOD_OS_CriticalSection_Enter( asyncthread->mCrit );
        found = true;
        FMOD_OS_CriticalSection_Leave( asyncthread->mCrit );
    }
    FMOD_OS_CriticalSection_Leave( lbl_80529020->mAsyncCrit );

    if ( !found ) {
#line 622
        asyncthread = new ( FMOD_Memory_Alloc( sizeof( AsyncThread ) ) ) AsyncThread;
        if ( !asyncthread ) {
            return FMOD_ERR_MEMORY;
        }

        result = asyncthread->init( false, sound ? sound->mSystem : NULL );
        if ( result != FMOD_OK ) {
            return result;
        }
    }

    if ( sound ) {
        sound->mAsyncData->mThread = asyncthread;
    }

    return FMOD_OK;
} // 0x8032C49C

} // namespace FMOD
