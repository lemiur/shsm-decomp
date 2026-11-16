/*
 * fmod_memory.cpp: FMOD::MemPool, FMOD's memory manager (an allocator on the operating system's heap
 * through dlmalloc, on callbacks the application gave, or on a bitmap of equal blocks), and
 * FMOD::MemSingleton, a block shared by its users.
 *
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_memory.obj). Retail is another revision
 * where MemPool::set is concerned (0x250 bytes against Gormiti's 0x268); there the retail assembly decides.
 * Gormiti's UNUSED members here (FMOD_Memory_allocC, FMOD_Memory_callocC, FMOD_Memory_reallocC,
 * FMOD_Memory_freeC, MemPool::initCustom, MemPool::getSize) have no copy in retail.
 */
// Unit name and boundary: Gormiti link map (fmodex.a fmod_memory.obj)

#include <stdio.h>
#include <string.h>

#include <fmod/fmod_memory.h>
#include <fmod/fmod_string.h>

namespace FMOD {

/*
====================
FindFreeBlocks

Looks for numblocks free blocks in a row in the bitmap of pool, from block startblock on, giving up at
endblock or the end of the pool; returns the offset of the first block of the run, or -1. A full word is
skipped 32 blocks at a time. Name and shape invented: retail inlines it into set, alloc and realloc.
====================
*/
static inline int FindFreeBlocks( MemPool *pool, int startblock, int numblocks, int endblock ) {
    int freecount = 0;
    int bitmask;
    int byteno;
    int blockno = startblock;

    byteno  = startblock >> 3;
    bitmask = 1 << ( startblock & 7 );

    while ( freecount < numblocks && blockno < endblock && blockno < pool->mSizeBlocks ) {
        if ( ( pool->mBitmap[byteno] & bitmask )
             || ( ( blockno & 31 ) == 0 && *(unsigned int *)&pool->mBitmap[byteno] == 0xFFFFFFFF ) ) {
            freecount = 0;
        } else {
            freecount++;
        }

        if ( ( blockno & 31 ) == 0 && *(unsigned int *)&pool->mBitmap[byteno] == 0xFFFFFFFF ) {
            byteno += 4;
            blockno += 32;
        } else {
            blockno++;
            bitmask <<= 1;
            if ( ( blockno & 7 ) == 0 ) {
                bitmask = 1;
                byteno++;
            }
        }
    }

    if ( freecount == numblocks ) {
        return blockno - numblocks;
    }
    return -1;
}

/*
====================
FindThreadSlot

Returns the slot of the pool's thread table that belongs to the calling thread, taking the first free one
for a thread not seen before (slot 0 is the total); the last slot when the table is full. Name and shape
invented: retail inlines it into alloc and realloc.
====================
*/
static inline int FindThreadSlot( MemPool *pool ) {
    unsigned int threadid = 0;
    int          count;

    FMOD_OS_Thread_GetCurrentID( &threadid );
    for ( count = 1; count < MemPool::MAX_THREADS; count++ ) {
        if ( threadid == pool->mCurrentAllocatedThreadID[count] ) {
            break;
        }
        if ( pool->mCurrentAllocatedThreadID[count] == 0 ) {
            pool->mCurrentAllocatedThreadID[count] = threadid;
            pool->mCurrentAllocated[count]         = 0;
            break;
        }
    }
    return count;
}

/*
====================
MemSingleton::alloc

The first user takes the block from the global pool; every user counts as one reference.
====================
*/
void *MemSingleton::alloc( int size, const char *file, int line ) {
    if ( !mRefCount ) {
        mBuffer = lbl_80529020->mMemPool->alloc( size, file, line, 0, false );
    }
    mRefCount++;

    return mBuffer;
} // 0x80375924

/*
====================
MemSingleton::free

Drops one reference and gives the block back to the global pool with the last.
====================
*/
void MemSingleton::free( const char *file, const int line ) {
    if ( mRefCount ) {
        mRefCount--;
    }
    if ( !mRefCount && mBuffer ) {
        lbl_80529020->mMemPool->free( mBuffer, file, line, 0 );
        mBuffer = NULL;
    }
} // 0x80375980

/*
====================
Memory_DefaultMalloc

The default allocation callbacks of a pool, on the operating system's heap.
====================
*/
void *Memory_DefaultMalloc( unsigned int size, FMOD_MEMORY_TYPE type ) {
    return FMOD_OS_Memory_Alloc( size );
} // 0x803759F8

/*
====================
Memory_DefaultRealloc
====================
*/
void *Memory_DefaultRealloc( void *ptr, unsigned int size, FMOD_MEMORY_TYPE type ) {
    return FMOD_OS_Memory_Realloc( ptr, size );
} // 0x803759FC

/*
====================
Memory_DefaultFree
====================
*/
void Memory_DefaultFree( void *ptr, FMOD_MEMORY_TYPE type ) {
    FMOD_OS_Memory_Free( ptr );
} // 0x80375A00

/*
====================
MemPool::MemPool

An empty pool on the default allocation callbacks.
====================
*/
MemPool::MemPool() {
    mAlloc   = Memory_DefaultMalloc;
    mRealloc = Memory_DefaultRealloc;
    mFree    = Memory_DefaultFree;

    mCrit = NULL;

    for ( int count = 0; count < MAX_THREADS; count++ ) {
        mCurrentAllocated[count]         = 0;
        mCurrentAllocatedThreadID[count] = 0;
    }

    mCurrentAllocatedSecondary = 0;
    mMaxAllocatedSecondary     = 0;
    mDLMallocSpace             = NULL;
} // 0x80375A04

/*
====================
MemPool::~MemPool
====================
*/
MemPool::~MemPool() {
    close();
} // 0x80375AD8

/*
====================
MemPool::init

Makes the block of user memory poolmem the pool, a dlmalloc space aligned to 256 bytes with a size that
is a multiple of blocksize.
====================
*/
FMOD_RESULT MemPool::init( void *poolmem, int poolsize, int blocksize ) {
    FMOD_RESULT result;
    void       *oldpoolmem = poolmem;

    if ( !poolmem || !poolsize ) {
        return FMOD_ERR_MEMORY;
    }

    close();

    poolmem = (void *)( ( (unsigned int)poolmem + 255 ) & ~255 );
    poolsize -= (char *)poolmem - (char *)oldpoolmem;
    poolsize &= ~( blocksize - 1 );

    mDLMallocSpace = create_mspace_with_base( poolmem, poolsize, 0 );
    if ( !mDLMallocSpace ) {
        return FMOD_ERR_MEMORY;
    }

    mNumBlocks                 = 0;
    mMaxBlocks                 = 0;
    mMaxAllocated              = 0;
    mMaxAllocatedSecondary     = 0;
    mActualMaxBytes            = 0;
    mWastage                   = 0;
    mFirstFreeBlock            = 0;
    mCurrentAllocatedSecondary = 0;

    for ( int count = 0; count < MAX_THREADS; count++ ) {
        mCurrentAllocated[count]         = 0;
        mCurrentAllocatedThreadID[count] = 0;
    }

    result = FMOD_OS_CriticalSection_Create( &mCrit, true );
    if ( result != FMOD_OK ) {
        return result;
    }

    return FMOD_OK;
} // 0x80375B2C

/*
====================
MemPool::close

Frees the bitmap, clears the pool's statistics and goes back to the default callbacks.
====================
*/
void MemPool::close() {
    if ( mCustomPool && mBitmap ) {
#line 343
        FMOD_Memory_Free( mBitmap );
    }

    mBitmap       = NULL;
    mData         = NULL;
    mSizeBytes    = 0;
    mSizeBlocks   = 0;
    mNumBlocks    = 0;
    mMaxBlocks    = 0;
    mMaxAllocated = 0;

    for ( int count = 0; count < MAX_THREADS; count++ ) {
        mCurrentAllocated[count]         = 0;
        mCurrentAllocatedThreadID[count] = 0;
    }

    mAlloc      = Memory_DefaultMalloc;
    mRealloc    = Memory_DefaultRealloc;
    mFree       = Memory_DefaultFree;
    mCustomPool = false;

    if ( mCrit ) {
        FMOD_OS_CriticalSection_Free( mCrit, true );
        mCrit = NULL;
    }
} // 0x80375C90

/*
====================
MemPool::set

Marks numblocks blocks from blockoffset as used (value set) or free (value clear), a bit at a time to a
word boundary, then a byte at a time, then the rest, and keeps mFirstFreeBlock on the first free block.

Not exact: register/schedule only, 3 rows (wall.exe). In the guard before the search retail keeps the
constant 1 in r3 and the shift amount in r4; the compile has them the other way round. Ruled out: the
operand order of the guard and twelve declaration orders of the search's locals (which make alloc and
realloc exact).
====================
*/
void MemPool::set( int blockoffset, int value, int numblocks ) {
    int byteoffset = blockoffset / 8;
    int bitoffset  = blockoffset & 7;
    int count      = 0;
    int blocksleft = numblocks;

    if ( blockoffset & 31 ) {
        count = 32 - ( blockoffset & 31 );
        if ( count > numblocks ) {
            count = numblocks;
        }
    }

    while ( count ) {
        if ( value ) {
            mBitmap[byteoffset] |= 1 << bitoffset;
        } else {
            mBitmap[byteoffset] &= ~( 1 << bitoffset );
        }
        bitoffset++;
        if ( bitoffset >= 8 ) {
            bitoffset = 0;
            byteoffset++;
        }
        blocksleft--;
        count--;
    }

    count = blocksleft / 8;
    if ( count ) {
        memset( &mBitmap[byteoffset], value ? 0xFF : 0, count );
        byteoffset += count;
        blocksleft -= count * 8;
    }

    count = blocksleft & 31;
    while ( count ) {
        if ( value ) {
            mBitmap[byteoffset] |= 1 << bitoffset;
        } else {
            mBitmap[byteoffset] &= ~( 1 << bitoffset );
        }
        bitoffset++;
        if ( bitoffset >= 8 ) {
            bitoffset = 0;
            byteoffset++;
        }
        count--;
    }

    if ( value ) {
        if ( blockoffset == mFirstFreeBlock ) {
            mFirstFreeBlock = blockoffset + numblocks;
        }
        if ( mBitmap[mFirstFreeBlock / 8] & ( 1 << ( mFirstFreeBlock & 7 ) ) ) {
            mFirstFreeBlock = FindFreeBlocks( this, mFirstFreeBlock, 1, mSizeBlocks );
        }
    } else {
        if ( blockoffset < mFirstFreeBlock ) {
            mFirstFreeBlock = blockoffset;
        }
    }
} // 0x80375DEC

/*
====================
MemPool::alloc

An allocation of len bytes, tagged with the caller's file and line: on the application's callback, on
dlmalloc or in a run of free blocks of the bitmap; the statistics of the pool follow it. A failure is
reported to the application's system callback.
====================
*/
void *MemPool::alloc( int len, const char *file, int line, FMOD_MEMORY_TYPE type, bool clear ) {
    MemBlockHeader *block     = NULL;
    int             numblocks = 0;
    int             reallen   = len;

    if ( !mCrit ) {
        FMOD_RESULT result = FMOD_OS_CriticalSection_Create( &mCrit, true );
        if ( result ) {
            return NULL;
        }
    }

    FMOD_OS_CriticalSection_Enter( mCrit );

    if ( !mCustomPool ) {
        reallen += sizeof( MemBlockHeader );
    }

    if ( mAlloc ) {
        type &= lbl_80529020->mMemoryTypeFlags;
        block = (MemBlockHeader *)mAlloc( reallen, type );
    } else if ( !mCustomPool ) {
        block = (MemBlockHeader *)mspace_malloc( mDLMallocSpace, reallen );
    } else {
        int blockoffset;

        numblocks = ( reallen + mBlockSize - 1 ) / mBlockSize;

        blockoffset = FindFreeBlocks( this, mFirstFreeBlock, numblocks, mSizeBlocks );

        if ( blockoffset >= 0 ) {
            set( blockoffset, 1, numblocks );

            if ( mCustomPool ) {
#line 692
                block = (MemBlockHeader *)FMOD_Memory_Alloc( sizeof( MemBlockHeader ) );
            } else {
                block = (MemBlockHeader *)( mData + blockoffset * mBlockSize );
            }

            block->mBlockOffset = blockoffset;
        }
    }

    if ( !block ) {
        char fileLine[256];

        FMOD_OS_CriticalSection_Leave( mCrit );

        if ( lbl_80529020->mSystemCallback ) {
            sprintf( fileLine, "%s (%d)", file, line );
            lbl_80529020->mSystemCallback( NULL, FMOD_SYSTEM_CALLBACKTYPE_MEMORYALLOCATIONFAILED, fileLine,
                                           (void *)len );
        }

        return NULL;
    }

    block->mSize      = len;
    block->mNumBlocks = numblocks;

    block->mThreadID = FindThreadSlot( this );

    if ( type & FMOD_MEMORY_SECONDARY ) {
        mCurrentAllocatedSecondary += block->mSize;
        if ( mCurrentAllocatedSecondary > mMaxAllocatedSecondary ) {
            mMaxAllocatedSecondary = mCurrentAllocatedSecondary;
        }
    } else {
        mCurrentAllocated[0] += block->mSize;
        mCurrentAllocated[block->mThreadID] += block->mSize;
        if ( mCurrentAllocated[0] > mMaxAllocated ) {
            mMaxAllocated = mCurrentAllocated[0];
        }
    }

    mNumBlocks += block->mNumBlocks;
    if ( mNumBlocks > mMaxBlocks ) {
        mMaxBlocks      = mNumBlocks;
        mActualMaxBytes = mNumBlocks * mBlockSize;
        mWastage        = mActualMaxBytes - mMaxAllocated;
    }

    if ( !mCustomPool ) {
        block++;
        if ( clear && block ) {
            memset( block, 0, len );
        }
    }

    FMOD_OS_CriticalSection_Leave( mCrit );

    return block;
} // 0x8037603C

/*
====================
MemPool::free

Returns an allocation to its callback, to dlmalloc or to the bitmap and takes it off the statistics.
====================
*/
void MemPool::free( void *ptr, const char *file, int line, FMOD_MEMORY_TYPE type ) {
    MemBlockHeader *block = (MemBlockHeader *)ptr;

    if ( !mCrit ) {
        FMOD_RESULT result = FMOD_OS_CriticalSection_Create( &mCrit, true );
        if ( result ) {
            return;
        }
    }

    FMOD_OS_CriticalSection_Enter( mCrit );

    if ( !mCustomPool ) {
        block--;
    }

    if ( type & FMOD_MEMORY_SECONDARY ) {
        mCurrentAllocatedSecondary -= block->mSize;
    } else {
        mCurrentAllocated[0] -= block->mSize;
        mCurrentAllocated[block->mThreadID] -= block->mSize;
    }

    mNumBlocks -= block->mNumBlocks;
    block->mThreadID = 0;

    if ( mFree ) {
        mFree( block, type & lbl_80529020->mMemoryTypeFlags );
    } else if ( !mCustomPool ) {
        mspace_free( mDLMallocSpace, block );
    } else {
        set( block->mBlockOffset, 0, block->mNumBlocks );
    }

    FMOD_OS_CriticalSection_Leave( mCrit );

    if ( mCustomPool ) {
#line 857
        FMOD_Memory_Free( block );
    }
} // 0x803763E4

/*
====================
MemPool::calloc
====================
*/
void *MemPool::calloc( int len, const char *file, int line, FMOD_MEMORY_TYPE type ) {
    return alloc( len, file, line, type, true );
} // 0x80376548

/*
====================
MemPool::realloc

Resizes an allocation, in place where the blocks after it are free, and keeps its contents. A NULL ptr is
an allocation; a failure is reported to the application's system callback.
====================
*/
void *MemPool::realloc( void *ptr, int len, const char *file, int line, FMOD_MEMORY_TYPE type ) {
    MemBlockHeader *block     = (MemBlockHeader *)ptr;
    int             reallen   = len;
    int             numblocks = 0;

    if ( !ptr ) {
        return alloc( len, file, line, 0, false );
    }

    if ( !mCrit ) {
        FMOD_RESULT result = FMOD_OS_CriticalSection_Create( &mCrit, true );
        if ( result ) {
            return NULL;
        }
    }

    FMOD_OS_CriticalSection_Enter( mCrit );

    if ( !mCustomPool ) {
        reallen += sizeof( MemBlockHeader );
        block--;
    }

    mCurrentAllocated[0] -= block->mSize;
    mCurrentAllocated[block->mThreadID] -= block->mSize;
    mNumBlocks -= block->mNumBlocks;

    if ( mRealloc ) {
        block = (MemBlockHeader *)mRealloc( block, reallen, type & lbl_80529020->mMemoryTypeFlags );
    } else if ( !mCustomPool ) {
        block = (MemBlockHeader *)mspace_realloc( mDLMallocSpace, block, reallen );
    } else {
        int blockoffset;

        numblocks = ( reallen + mBlockSize - 1 ) / mBlockSize;

        set( block->mBlockOffset, 0, block->mNumBlocks );

        blockoffset = FindFreeBlocks( this, block->mBlockOffset, numblocks, block->mBlockOffset + numblocks );

        if ( blockoffset >= 0 ) {
            set( blockoffset, 1, numblocks );
            block               = (MemBlockHeader *)( mData + blockoffset * mBlockSize );
            block->mBlockOffset = blockoffset;
        } else {
            blockoffset = FindFreeBlocks( this, mFirstFreeBlock, numblocks, mSizeBlocks );

            if ( blockoffset >= 0 ) {
                MemBlockHeader *newblock;

                set( blockoffset, 1, numblocks );
                if ( mCustomPool ) {
                    newblock = block;
                } else {
                    newblock = (MemBlockHeader *)( mData + blockoffset * mBlockSize );
                }
                newblock->mBlockOffset = blockoffset;
                if ( !mCustomPool ) {
                    FMOD_memmove( newblock + 1, block + 1, block->mSize );
                }
                block = newblock;
            } else {
                block = NULL;
            }
        }
    }

    if ( !block ) {
        char fileLine[256];

        FMOD_OS_CriticalSection_Leave( mCrit );

        if ( lbl_80529020->mSystemCallback ) {
            sprintf( fileLine, "%s (%d)", file, line );
            lbl_80529020->mSystemCallback( NULL, FMOD_SYSTEM_CALLBACKTYPE_MEMORYALLOCATIONFAILED, fileLine,
                                           (void *)len );
        }

        return NULL;
    }

    block->mSize      = len;
    block->mNumBlocks = numblocks;

    block->mThreadID = FindThreadSlot( this );

    mCurrentAllocated[0] += block->mSize;
    mCurrentAllocated[block->mThreadID] += block->mSize;
    if ( mCurrentAllocated[0] > mMaxAllocated ) {
        mMaxAllocated = mCurrentAllocated[0];
    }

    mNumBlocks += block->mNumBlocks;
    if ( mNumBlocks > mMaxBlocks ) {
        mMaxBlocks      = mNumBlocks;
        mActualMaxBytes = mNumBlocks * mBlockSize;
        mWastage        = mActualMaxBytes - mMaxAllocated;
    }

    if ( !mCustomPool ) {
        block++;
    }

    FMOD_OS_CriticalSection_Leave( mCrit );

    return block;
} // 0x80376550

} // namespace FMOD
