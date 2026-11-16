/*
 * fmod_speakerlevels_pool.cpp: FMOD::SpeakerLevelsPool, the system's pool of speaker level buffers.
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_speakerlevels_pool.obj)

#include <fmod/fmod.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_speakerlevels_pool.h>
#include <fmod/fmod_systemi.h>
#include <string.h>

namespace FMOD {

/*
====================
SpeakerLevelsPool::SpeakerLevelsPool

An empty pool; the system is set when the pool is first used.
====================
*/
SpeakerLevelsPool::SpeakerLevelsPool() {
    mLevelsPool = NULL;
} // 0x80349CFC

/*
====================
SpeakerLevelsPool::alloc

Hands out a cleared buffer of levels. The first call makes the table of entries; later calls reuse an
entry whose buffer is free, or give the first entry without a buffer one.
====================
*/
FMOD_RESULT SpeakerLevelsPool::alloc( float **levels ) {
    int freepos            = mSystem->mNumChannels;
    int numoutputchannels  = mSystem->mMaxOutputChannels;

    if ( !mLevelsPool ) {
#line 52
        mLevelsPool = (LevelsInfo *)FMOD_Memory_Calloc( mSystem->mNumChannels * sizeof( LevelsInfo ) );
        if ( !mLevelsPool ) {
            return FMOD_ERR_MEMORY;
        }
    }

    // A prologic output carries six channels of levels.
    if ( mSystem->mSpeakerMode == FMOD_SPEAKERMODE_PROLOGIC ) {
        numoutputchannels = 6;
    }

    for ( int i = 0; i < mSystem->mNumChannels; i++ ) {
        if ( !mLevelsPool[i].inuse && mLevelsPool[i].levelsmemory ) {
            memset( mLevelsPool[i].levelsmemory, 0, mSystem->mMaxInputChannels * ( numoutputchannels * sizeof( float ) ) );

            mLevelsPool[i].inuse = true;
            *levels = mLevelsPool[i].levelsmemory;

            return FMOD_OK;
        }

        if ( !mLevelsPool[i].levelsmemory && i < freepos ) {
            freepos = i;
        }
    }

#line 97
    mLevelsPool[freepos].levelsmemory = (float *)FMOD_Memory_Calloc( mSystem->mMaxInputChannels * ( numoutputchannels * sizeof( float ) ) );
    if ( !mLevelsPool[freepos].levelsmemory ) {
        return FMOD_ERR_MEMORY;
    }

    mLevelsPool[freepos].inuse = true;
    *levels = mLevelsPool[freepos].levelsmemory;

    return FMOD_OK;
} // 0x80349D08

/*
====================
SpeakerLevelsPool::free

Gives a buffer back to the pool; the memory stays with its entry.
====================
*/
FMOD_RESULT SpeakerLevelsPool::free( float *levels ) {
    if ( mLevelsPool ) {
        for ( int i = 0; i < mSystem->mNumChannels; i++ ) {
            if ( mLevelsPool[i].levelsmemory == levels ) {
                mLevelsPool[i].inuse = false;
                break;
            }
        }
    }

    return FMOD_OK;
} // 0x80349EC0

/*
====================
SpeakerLevelsPool::release

Frees every buffer and the table of entries.
====================
*/
FMOD_RESULT SpeakerLevelsPool::release() {
    if ( mLevelsPool ) {
        for ( int i = 0; i < mSystem->mNumChannels; i++ ) {
            if ( mLevelsPool[i].levelsmemory ) {
#line 166
                FMOD_Memory_Free( mLevelsPool[i].levelsmemory );
                mLevelsPool[i].levelsmemory = NULL;
            }
        }

#line 171
        FMOD_Memory_Free( mLevelsPool );
        mLevelsPool = NULL;
    }

    return FMOD_OK;
} // 0x80349F1C

} // namespace FMOD
