/*
 * FMOD::SimpleMemPool: the Event system's bump allocator over one block of memory. A null pool
 * passes its allocations on to the global pool. Members and layout from Gormiti's DWARF
 * (sibling.exe types SimpleMemPool).
 */
#ifndef _FMOD_SIMPLEMEMPOOL_H
#define _FMOD_SIMPLEMEMPOOL_H

namespace FMOD {

class SimpleMemPool {
public:
    int   mSize;        // 0x00, bytes in the block
    char *mBuffer;      // 0x04, the block
    char *mNextBlock;   // 0x08, where the next allocation starts

    SimpleMemPool();
    // Frees the block (when the pool allocated it) and the pool itself.
    void release();
    // Sets the pool up over buffer, or over a fresh block of sizebytes when buffer is NULL.
    bool init( int sizebytes, void *buffer );
    void *alloc( int sizebytes, const char *file, const int line );
    void *calloc( int sizebytes, const char *file, const int line );
    // A bump allocator frees nothing; only the global pool does (this == NULL).
    void free( void *ptr, const char *file, const int line );
};

} // namespace FMOD

#endif /* _FMOD_SIMPLEMEMPOOL_H */
