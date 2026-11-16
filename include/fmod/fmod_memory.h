/*
 * FMOD's memory pool and global state. Class names invented (FMOD convention); members are named
 * from their use in retail where it is clear and by offset where it is not.
 */
#ifndef _FMOD_MEMORY_H
#define _FMOD_MEMORY_H

#include "fmod.h"
#include "fmod_linkedlist.h"
#include "fmod_os_misc.h"
#include "fmod_channeli.h"
#include "fmod_types.h"
#include "dlmalloc.h"

namespace FMOD {

/*
 * A memory pool: either a fixed block of user memory carved into blocks, or a front for
 * allocation callbacks. Its size is not known yet.
 */
class MemPool {
public:
    enum { MAX_THREADS = 32 };   // slots of the thread table (name invented; the arrays are 32 long)

    // Sets the pool up over a block of user memory.
    FMOD_RESULT init( void *poolmem, int poollen, int blocksize );
    // A zero-filled allocation, tagged with the source file and line.
    void *calloc( int len, const char *file, int line, FMOD_MEMORY_TYPE type );
    // An allocation, zero-filled when clear is set.
    void *alloc( int len, const char *file, int line, FMOD_MEMORY_TYPE type, bool clear );
    // Resizes an allocation of this pool, keeping its contents; a NULL ptr allocates.
    void *realloc( void *ptr, int len, const char *file, int line, FMOD_MEMORY_TYPE type );
    // Frees an allocation of this pool.
    void free( void *ptr, const char *file, int line, FMOD_MEMORY_TYPE type );
    // Releases the pool's memory and goes back to the default allocation callbacks.
    void close();
    // Marks numblocks blocks from blockoffset as used (value set) or free (value clear) in the bitmap.
    void set( int blockoffset, int value, int numblocks );

    // Name invented: installs the allocation callbacks (all NULL for a pool of user memory).
    void setCallbacks( FMOD_MEMORY_ALLOCCALLBACK useralloc, FMOD_MEMORY_REALLOCCALLBACK userrealloc,
                       FMOD_MEMORY_FREECALLBACK userfree ) {
        mAlloc   = useralloc;
        mRealloc = userrealloc;
        mFree    = userfree;
    }

    // Members up to 0x138: Gormiti DWARF (sibling.exe types FMOD::MemPool).
    unsigned char              *mBitmap;                        // 0x00
    unsigned char              *mData;                          // 0x04
    bool                        mCustomPool;                    // 0x08
    int                         mSizeBytes;                     // 0x0C
    int                         mSizeBlocks;                    // 0x10
    int                         mNumBlocks;                     // 0x14
    int                         mMaxBlocks;                     // 0x18
    unsigned int                mCurrentAllocated[MAX_THREADS + 1]; // 0x1C, a counter per slot of the thread table
    unsigned int                mCurrentAllocatedThreadID[MAX_THREADS]; // 0xA0, the thread that owns each slot (0 = free)
    unsigned int                mMaxAllocated;                  // 0x120
    unsigned int                mMaxAllocatedSecondary;         // 0x124
    unsigned int                mCurrentAllocatedSecondary;     // 0x128
    int                         mActualMaxBytes;                // 0x12C
    int                         mWastage;                       // 0x130
    int                         mFirstFreeBlock;                // 0x134
    FMOD_MEMORY_ALLOCCALLBACK   mAlloc;     // 0x138, NULL when the pool owns user memory
    FMOD_MEMORY_REALLOCCALLBACK mRealloc;   // 0x13C
    FMOD_MEMORY_FREECALLBACK    mFree;      // 0x140

    // ---- fmod_globals: the pool's full size and its lifetime (Gormiti DWARF: size 0x150) ----
    void                       *mDLMallocSpace; // 0x144
    FMOD_OS_CRITICALSECTION    *mCrit;          // 0x148
    int                         mBlockSize;     // 0x14C

    MemPool();                              // 0x80375A04
    ~MemPool();                             // 0x80375AD8
};

/*
 * The header in front of each allocation of a MemPool (Gormiti DWARF: size 0x10). A pool that keeps its
 * blocks in a bitmap holds the offset of the first block in mBlockOffset.
 */
class MemBlockHeader {
public:
    int          mSize;         // 0x00, bytes asked for
    int          mNumBlocks;    // 0x04, blocks taken (0 unless the pool is a bitmap)
    int          mBlockOffset;  // 0x08
    unsigned int mThreadID;     // 0x0C, the slot of the thread table of the allocating thread
};

/*
 * A block of memory shared by its users and freed with the last of them (Gormiti DWARF: size 8).
 */
class MemSingleton {
public:
    void *mBuffer;      // 0x0
    int   mRefCount;    // 0x4

    ~MemSingleton() {
        free( "", 0 );
    }

    void       *alloc( int size, const char *file, int line );   // 0x80375924
    void        free( const char *file, int line );              // 0x80375980
};

/*
 * FMOD's global state, reached through one pointer (lbl_80529020), defined in fmod_globals.cpp.
 * Layout: Gormiti DWARF (Global, size 0x58); members past 0x38 keep placeholder names.
 */
struct Global {
    LinkedListNode  *mSystemHead;       // 0x0, head of the list of created systems
    MemPool         *mMemPool;          // 0x4
    int              mSystemInitCount;  // 0x8
    unsigned char    field_0xC[0x10 - 0xC];
    FMOD_UINT64P     mDSPClock;         // 0x10
    unsigned int     mDSPClockTimeStamp; // 0x18
    int              mFileBusy;         // 0x1C
    FMOD_SYSTEM_CALLBACK mSystemCallback; // 0x20, NULL until the application sets one
    FMOD_MEMORY_TYPE mMemoryTypeFlags;  // 0x24, set by FMOD_Memory_Initialize
    LinkedListNode   mFileThreadHead;   // 0x28
    FMOD_OS_CRITICALSECTION *mFileCrit; // 0x34
    FMOD_OS_CRITICALSECTION *mAsyncCrit; // 0x38, guards the async thread list
    malloc_params    mDLMallocParams;   // 0x3C, dlmalloc's parameters (dlmalloc.cpp points mparams here)
    int              mRandomValue;      // 0x54

    Global() {
        init();
    }

    void init();                        // sets the state up (fmod_globals.cpp)
};

// The default allocation callbacks, on the operating system's heap (names invented).
void *Memory_DefaultMalloc( unsigned int size, FMOD_MEMORY_TYPE type );
void *Memory_DefaultRealloc( void *ptr, unsigned int size, FMOD_MEMORY_TYPE type );
void  Memory_DefaultFree( void *ptr, FMOD_MEMORY_TYPE type );

} // namespace FMOD

extern FMOD::Global *lbl_80529020;

// Allocation through the global pool, tagged with the caller's file and line.
#define FMOD_Memory_Alloc( _len )   lbl_80529020->mMemPool->alloc( _len, __FILE__, __LINE__, 0, false )
#define FMOD_Memory_Calloc( _len )  lbl_80529020->mMemPool->calloc( _len, __FILE__, __LINE__, 0 )
#define FMOD_Memory_CallocType( _len, _type ) lbl_80529020->mMemPool->calloc( _len, __FILE__, __LINE__, _type )
#define FMOD_Memory_Free( _ptr )    lbl_80529020->mMemPool->free( _ptr, __FILE__, __LINE__, 0 )
#define FMOD_Memory_ReallocType( _ptr, _len, _type ) lbl_80529020->mMemPool->realloc( _ptr, _len, __FILE__, __LINE__, _type )

// Frees an allocation of the global pool when there is one. Name invented; retail inlines it at every
// cleanup of EventProjectI::loadSampleData with the tag "fmod_memory.h", line 245 (the #line below).
inline void FMOD_Memory_FreeNotNull( void *ptr ) {
    if ( ptr ) {
#line 245
        FMOD_Memory_Free( ptr );
    }
}

#endif /* _FMOD_MEMORY_H */
