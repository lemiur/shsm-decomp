/*
 * fmod_dsp_connectionpool.cpp: FMOD::DSPConnectionPool, the system's pool of DSP connections.
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_connectionpool.obj)
 * Allocation tags carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */

#include <fmod/fmod.h>
#include <fmod/fmod_dsp_connectioni.h>
#include <fmod/fmod_dsp_connectionpool.h>
#include <fmod/fmod_memory.h>
#include <fmod/fmod_os_misc.h>
#include <fmod/fmod_systemi.h>
#include <new>

// Connections are carved out of a 16-byte aligned block.
#define ALIGN16( _ptr ) ( (unsigned int)( _ptr ) + 15 & ~15 )

// Number of slots in each of the pool's block tables.
#define MAX_BLOCKS 128

namespace FMOD {

/*
====================
DSPConnectionPool::init

Allocates the first block of connections, their list nodes and their level data, and puts every
connection on the free list. The requested count is rounded up to a whole number of 128.

Each connection's address is taken in its own statement before the placement new (Gormiti's line
table gives the two their own lines).
====================
*/
FMOD_RESULT DSPConnectionPool::init( SystemI *system, int numconnections, int numoutputlevels, int numinputlevels ) {
    float *leveldata;
    int    count;

    if ( numconnections < 0 ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    for ( int i = 0; i < MAX_BLOCKS; i++ ) {
        mConnection[i] = NULL;
        mLevelData[i]  = NULL;
    }

    mNumConnections = ( numconnections + 128 ) / 128 * 128;

#line 52
    mConnectionMemory[0] = (DSPConnectionI *)FMOD_Memory_Calloc( mNumConnections * sizeof( DSPConnectionI ) + 16 );
    if ( !mConnectionMemory[0] ) {
        return FMOD_ERR_MEMORY;
    }
    mConnection[0] = (DSPConnectionI *)ALIGN16( mConnectionMemory[0] );

#line 59
    mNodeMemory[0] = (LinkedListNode *)FMOD_Memory_Calloc( mNumConnections * sizeof( LinkedListNode ) );
    if ( !mNodeMemory[0] ) {
        return FMOD_ERR_MEMORY;
    }

    mNumOutputLevels = numoutputlevels;
    mNumInputLevels  = numinputlevels;

#line 77
    mLevelDataMemory[0] = (float *)FMOD_Memory_Calloc( mNumConnections * ( numoutputlevels < 2 ? 2 : numoutputlevels ) * ( numinputlevels < numoutputlevels ? numoutputlevels : numinputlevels ) * sizeof( LinkedListNode ) );
    if ( !mLevelDataMemory[0] ) {
        return FMOD_ERR_MEMORY;
    }
    mLevelData[0] = mLevelDataMemory[0];
    leveldata     = mLevelData[0];

    mFreeListHead.initNode();

    for ( count = 0; count < mNumConnections; count++ ) {
        DSPConnectionI *conn = &mConnection[0][count];

        new ( conn ) DSPConnectionI;

        conn->init( leveldata, numoutputlevels, numinputlevels );

        conn->mNode = &mNodeMemory[0][count];
        conn->mNode->setData( conn );
        conn->mNode->addAfter( &mFreeListHead );
    }

    mSystem = system;

    return FMOD_OK;
} // 0x8035D474

/*
====================
DSPConnectionPool::close

Frees every block of connections, nodes and level data.
====================
*/
FMOD_RESULT DSPConnectionPool::close() {
    int i;

    for ( i = 0; i < MAX_BLOCKS; i++ ) {
        if ( mConnectionMemory[i] ) {
#line 131
            FMOD_Memory_Free( mConnectionMemory[i] );
            mConnectionMemory[i] = NULL;
        }
        mConnection[i] = NULL;

        if ( mLevelDataMemory[i] ) {
#line 138
            FMOD_Memory_Free( mLevelDataMemory[i] );
            mLevelDataMemory[i] = NULL;
        }
        mLevelData[i] = NULL;

        if ( mNodeMemory[i] ) {
#line 145
            FMOD_Memory_Free( mNodeMemory[i] );
            mNodeMemory[i] = NULL;
        }
    }

    return FMOD_OK;
} // 0x8035D6F8

/*
====================
DSPConnectionPool::alloc

Takes a connection off the free list and files it on the used list. When the free list is empty a
new block of connections is allocated first.
====================
*/
FMOD_RESULT DSPConnectionPool::alloc( DSPConnectionI **connection, bool protect ) {
    LocalCriticalSection crit( mSystem->mDSPConnectionCrit );
    DSPConnectionI      *newconnection;

    if ( !mSystem ) {
        return FMOD_ERR_UNINITIALIZED;
    }

    if ( !connection ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( protect ) {
        crit.enter();
    }

    if ( mFreeListHead.isEmpty() ) {
        int    newblock;
        int    count;
        float *leveldata = NULL;

        for ( newblock = 0; newblock < MAX_BLOCKS; newblock++ ) {
            if ( !mConnectionMemory[newblock] ) {
                break;
            }
        }

        if ( newblock >= MAX_BLOCKS ) {
            return FMOD_ERR_INTERNAL;
        }

#line 208
        mConnectionMemory[newblock] = (DSPConnectionI *)FMOD_Memory_Calloc( mNumConnections * sizeof( DSPConnectionI ) + 16 );
        if ( !mConnectionMemory[newblock] ) {
            return FMOD_ERR_MEMORY;
        }
        mConnection[newblock] = (DSPConnectionI *)ALIGN16( mConnectionMemory[newblock] );

#line 218
        mNodeMemory[newblock] = (LinkedListNode *)FMOD_Memory_Calloc( mNumConnections * sizeof( LinkedListNode ) );
        if ( !mNodeMemory[newblock] ) {
            return FMOD_ERR_MEMORY;
        }

#line 230
        mLevelDataMemory[newblock] = (float *)FMOD_Memory_Calloc( mNumConnections * ( mNumOutputLevels < 2 ? 2 : mNumOutputLevels ) * ( mNumInputLevels < mNumOutputLevels ? mNumOutputLevels : mNumInputLevels ) * sizeof( LinkedListNode ) );
        if ( !mLevelDataMemory[newblock] ) {
            return FMOD_ERR_MEMORY;
        }
        mLevelData[newblock] = mLevelDataMemory[newblock];
        leveldata            = mLevelData[newblock];

        for ( count = 0; count < mNumConnections; count++ ) {
            DSPConnectionI *conn = &mConnection[newblock][count];

            new ( conn ) DSPConnectionI;

            conn->init( leveldata, mNumOutputLevels, mNumInputLevels );

            conn->mNode = &mNodeMemory[newblock][count];
            conn->mNode->setData( conn );
            conn->mNode->addAfter( &mFreeListHead );
        }
    }

    newconnection = (DSPConnectionI *)mFreeListHead.getNext()->getData();

    newconnection->mInputNode.setData( newconnection );
    newconnection->mOutputNode.setData( newconnection );

    newconnection->mNode->removeNode();
    newconnection->mNode->addAfter( &mUsedListHead );

    if ( protect ) {
        crit.leave();
    }

    *connection = newconnection;

    return FMOD_OK;
} // 0x8035D7D4

/*
====================
DSPConnectionPool::free

Unlinks a connection from both its units and puts it back on the free list.
====================
*/
FMOD_RESULT DSPConnectionPool::free( DSPConnectionI *connection, bool protect ) {
    LocalCriticalSection crit( mSystem->mDSPConnectionCrit );

    if ( !connection ) {
        return FMOD_ERR_INVALID_PARAM;
    }

    if ( protect ) {
        crit.enter();
    }

    connection->mInputNode.removeNode();
    connection->mOutputNode.removeNode();

    connection->mInputUnit  = NULL;
    connection->mOutputUnit = NULL;

    connection->mNode->removeNode();
    connection->mNode->addAfter( &mFreeListHead );
    connection->mNode->setData( connection );

    if ( protect ) {
        crit.leave();
    }

    return FMOD_OK;
} // 0x8035DBFC

} // namespace FMOD
