/*
 * fmod_simplemempool.cpp: FMOD::SimpleMemPool, the Event system's bump allocator. Allocation tags carry
 * the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_simplemempool.obj)

#include <string.h>

#include <fmod/fmod_memory.h>
#include <fmod/fmod_simplemempool.h>

namespace FMOD {

/*
====================
SimpleMemPool::SimpleMemPool

An empty pool.
====================
*/
SimpleMemPool::SimpleMemPool() {
    mSize      = 0;
    mBuffer    = NULL;
    mNextBlock = NULL;
} // 0x803B0204

/*
====================
SimpleMemPool::release

Frees the block, when there is one, and the pool.
====================
*/
void SimpleMemPool::release() {
    if ( mBuffer ) {
#line 26
        FMOD_Memory_Free( mBuffer );
    }

#line 29
    FMOD_Memory_Free( this );
} // 0x803B0218

/*
====================
SimpleMemPool::init

Takes buffer as the block, or allocates one when none is given.
====================
*/
bool SimpleMemPool::init( int sizebytes, void *buffer ) {
    mSize   = sizebytes;
    mBuffer = (char *)buffer;

    if ( !buffer ) {
#line 40
        buffer  = FMOD_Memory_Alloc( sizebytes );
        mBuffer = (char *)buffer;
        if ( !buffer ) {
            return false;
        }
    }

    mNextBlock = (char *)buffer;

    return true;
} // 0x803B0288

/*
====================
SimpleMemPool::alloc

Carves a block, rounded up to 4 bytes, off the end of the used part; NULL when the block is full.
A null pool allocates from the global pool.
====================
*/
void *SimpleMemPool::alloc( int sizebytes, const char *file, const int line ) {
    void *p = NULL;

    if ( this ) {
        if ( sizebytes ) {
            sizebytes = ( sizebytes + 3 ) & ~3;

            if ( mNextBlock < mBuffer + mSize && mNextBlock + sizebytes <= mBuffer + mSize ) {
                p = mNextBlock;
                mNextBlock += sizebytes;
            }
        }
    } else {
#line 76
        p = FMOD_Memory_Alloc( sizebytes );
    }

    return p;
} // 0x803B0300

/*
====================
SimpleMemPool::calloc

A zero-filled alloc.
====================
*/
void *SimpleMemPool::calloc( int sizebytes, const char *file, const int line ) {
    void *p;

    if ( this ) {
        p = alloc( sizebytes, file, line );
        if ( p ) {
            memset( p, 0, sizebytes );
        }
    } else {
#line 97
        p = FMOD_Memory_Calloc( sizebytes );
    }

    return p;
} // 0x803B0390

/*
====================
SimpleMemPool::free

Frees nothing in a pool; a null pool hands the block to the global pool.
====================
*/
void SimpleMemPool::free( void *ptr, const char *file, const int line ) {
    if ( !this ) {
#line 137
        FMOD_Memory_Free( ptr );
    }
} // 0x803B0474

} // namespace FMOD
