/*
 * fmod.cpp: FMOD Ex's C API, the extern "C" entry points that check a handle and forward to the
 * C++ API. Unit named from its "fmod.cpp" strings (FMOD_System_Create's allocation tags); the C
 * functions retail does not call were stripped from the link and have no copy here.
 */

#include <fmod/fmod.h>
#include <fmod/fmod.hpp>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_systemi.h>
#include <new>
#include <string.h>

// A pool given to FMOD_Memory_Initialize is managed in blocks of this many bytes (name invented).
#define MEMPOOL_BLOCKSIZE       256

// Set in the memory type flags by FMOD_Memory_Initialize; meaning unknown (name invented).
#define MEMORY_TYPE_0x100000    0x00100000

// Systems carry an index from 1 to MAX_SYSTEMS - 1 (name invented).
#define MAX_SYSTEMS             16

/*
====================
FMOD_Memory_Initialize

Sets up FMOD's memory before any system exists: either a fixed pool of user memory, a set of
user callbacks, or (with neither) the default callbacks.
====================
*/
extern "C" FMOD_RESULT F_API FMOD_Memory_Initialize( void *poolmem, int poollen, FMOD_MEMORY_ALLOCCALLBACK useralloc,
                                                     FMOD_MEMORY_REALLOCCALLBACK userrealloc,
                                                     FMOD_MEMORY_FREECALLBACK userfree, FMOD_MEMORY_TYPE memtypeflags ) {
    if ( !lbl_80529020->mSystemHead->isEmpty() ) {
        return FMOD_ERR_INITIALIZED;
    }
    if ( poollen % MEMPOOL_BLOCKSIZE ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    lbl_80529020->mMemoryTypeFlags = memtypeflags;
    lbl_80529020->mMemoryTypeFlags |= MEMORY_TYPE_0x100000;

    if ( poollen && poolmem ) {
        FMOD_RESULT result;

        if ( useralloc || userrealloc || userfree ) {
            return FMOD_ERR_INVALID_PARAM;
        }
        if ( poollen < MEMPOOL_BLOCKSIZE ) {
            return FMOD_ERR_INVALID_PARAM;
        }

        result = lbl_80529020->mMemPool->init( poolmem, poollen, MEMPOOL_BLOCKSIZE );
        if ( result == FMOD_OK ) {
            lbl_80529020->mMemPool->setCallbacks( NULL, NULL, NULL );
        }
        return result;
    }

    if ( poolmem || poollen ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( useralloc && userrealloc && userfree ) {
        lbl_80529020->mMemPool->setCallbacks( useralloc, userrealloc, userfree );
    } else if ( !useralloc && !userrealloc && !userfree ) {
        lbl_80529020->mMemPool->setCallbacks( FMOD::Memory_DefaultMalloc, FMOD::Memory_DefaultRealloc, FMOD::Memory_DefaultFree );
    } else {
        return FMOD_ERR_INVALID_PARAM;
    }

    return FMOD_OK;
} // 0x8032BA54

/*
====================
FMOD_System_Create

Creates a system and gives it the lowest index no other system holds.
====================
*/
// Retail's allocation tags carry the original file's line numbers; the functions before this one in
// that file have no copy in retail, so the two tagged statements give their lines.
extern "C" FMOD_RESULT F_API FMOD_System_Create( FMOD_SYSTEM **system ) {
    FMOD::SystemI *newsystem;
    FMOD::SystemI *current;
    bool           used[MAX_SYSTEMS];
    int            index;

    if ( !system ) {
        return FMOD_ERR_INVALID_PARAM;
    }

#line 251
    newsystem = new ( FMOD_Memory_Calloc( sizeof( FMOD::SystemI ) ) ) FMOD::SystemI;
    *system = (FMOD_SYSTEM *)newsystem;
    if ( !newsystem ) {
        return FMOD_ERR_MEMORY;
    }

    memset( used, 0, sizeof( used ) );
    for ( current = static_cast< FMOD::SystemI * >( lbl_80529020->mSystemHead->getNext() );
          current != lbl_80529020->mSystemHead;
          current = static_cast< FMOD::SystemI * >( current->getNext() ) ) {
        used[current->mIndex - 1] = true;
    }

    for ( index = 0; index < MAX_SYSTEMS - 1; index++ ) {
        if ( !used[index] ) {
            newsystem->mIndex = index + 1;
            break;
        }
    }
    if ( index == MAX_SYSTEMS - 1 ) {
#line 276
        FMOD_Memory_Free( newsystem );
        return FMOD_ERR_MEMORY;
    }

    newsystem->addAfter( lbl_80529020->mSystemHead );
    return FMOD_OK;
} // 0x8032BBFC

/*
====================
FMOD_Sound_GetSyncPoint
====================
*/
extern "C" FMOD_RESULT F_API FMOD_Sound_GetSyncPoint( FMOD_SOUND *sound, int index, FMOD_SYNCPOINT **point ) {
    FMOD::Sound *_sound = (FMOD::Sound *)sound;

    if ( !_sound ) {
        return FMOD_ERR_INVALID_PARAM;
    }
    return _sound->getSyncPoint( index, point );
} // 0x8032BD5C

/*
====================
FMOD_Sound_GetSyncPointInfo
====================
*/
extern "C" FMOD_RESULT F_API FMOD_Sound_GetSyncPointInfo( FMOD_SOUND *sound, FMOD_SYNCPOINT *point, char *name,
                                                          int namelen, unsigned int *offset,
                                                          FMOD_TIMEUNIT offsettype ) {
    FMOD::Sound *_sound = (FMOD::Sound *)sound;

    if ( !_sound ) {
        return FMOD_ERR_INVALID_PARAM;
    }
    return _sound->getSyncPointInfo( point, name, namelen, offset, offsettype );
} // 0x8032BD70

/*
====================
FMOD_Channel_GetCurrentSound
====================
*/
extern "C" FMOD_RESULT F_API FMOD_Channel_GetCurrentSound( FMOD_CHANNEL *channel, FMOD_SOUND **sound ) {
    FMOD::Channel *_channel = (FMOD::Channel *)channel;

    if ( !_channel ) {
        return FMOD_ERR_INVALID_PARAM;
    }
    return _channel->getCurrentSound( (FMOD::Sound **)sound );
} // 0x8032BD84

/*
====================
FMOD_Channel_GetUserData
====================
*/
extern "C" FMOD_RESULT F_API FMOD_Channel_GetUserData( FMOD_CHANNEL *channel, void **userdata ) {
    FMOD::Channel *_channel = (FMOD::Channel *)channel;

    if ( !_channel ) {
        return FMOD_ERR_INVALID_PARAM;
    }
    return _channel->getUserData( userdata );
} // 0x8032BD98
