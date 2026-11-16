#ifndef CLIMAX_FULLHEAPALLOCATOR_H
#define CLIMAX_FULLHEAPALLOCATOR_H

#include <revolution/types.h>

/*
 * The header of one block of a CFullHeapAllocator heap. The blocks form a doubly linked list in address
 * order; the block's payload follows the record. Size counts 16-byte units, the record's own unit excluded.
 * Names and layout: Gormiti DWARF (FullHeapAllocator.cpp).
 */
struct StCoreHeapRecord {
    StCoreHeapRecord*   Next;
    StCoreHeapRecord*   Prev;
    int                 Size;
    int                 Flags;
};

/* StCoreHeapRecord::Flags: the block is free. Name invented. */
#define HEAP_RECORD_FREE    0x10000000

/*
 * A first-fit allocator over one contiguous heap, kept as a list of StCoreHeapRecord blocks.
 * Names and layout: Gormiti link map and DWARF (FullHeapAllocator.cpp).
 */
class CFullHeapAllocator {
public:
    CFullHeapAllocator();
    CFullHeapAllocator( void* Heap, int HeapSize );

    void*   Alloc( int size );
    // Gormiti's AllocTop( int ) has no copy in retail: not declared.
    void*   ReAlloc( void* mem, int size );
    bool    Free( void* mem );

    // Gormiti's Init( void*, int ) has no out-of-line copy in retail; the constructor holds its body.
    void    Init( void* Heap, int HeapSize ) {
        m_Heap = (StCoreHeapRecord*)Heap;
        m_Heap->Next = NULL;
        HeapSize = ( HeapSize & ~15 ) >> 4;
        m_Heap->Prev = NULL;
        m_Heap->Size = HeapSize - 1;
        m_Heap->Flags = HEAP_RECORD_FREE;
        m_HeapTop = m_Heap + ( m_Heap->Size + 1 );
    }

private:
    StCoreHeapRecord*   m_Heap;
    void*               m_HeapTop;
    int                 m_FailedAlloc_Count;
    int                 m_FailedAlloc_Size;
};

#endif
