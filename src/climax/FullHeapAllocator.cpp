// Unit name and boundary: Gormiti link map (FullHeapAllocator.obj).
// Names and layout: Gormiti link map and DWARF (SpiritFrameWork/memoryhandler/FullHeapAllocator.cpp).
//
// First-fit allocator over one contiguous heap. Gormiti's MemStat, TotalFree, TotalAlloc, Report and Dump
// have no copy in retail (another revision).

#include <climax/FullHeapAllocator.h>

/*
====================
CFullHeapAllocator::CFullHeapAllocator
====================
*/
// 0x801AC530
CFullHeapAllocator::CFullHeapAllocator()
    : m_Heap( NULL ), m_HeapTop( NULL ) {
}

/*
====================
CFullHeapAllocator::CFullHeapAllocator
====================
*/
// 0x801AC540
CFullHeapAllocator::CFullHeapAllocator( void* Heap, int HeapSize ) {
    Init( Heap, HeapSize );
}

/*
====================
CFullHeapAllocator::Alloc

Takes the first free block that is large enough, splitting off the remainder.
====================
*/
// 0x801AC590
void* CFullHeapAllocator::Alloc( int size ) {
    int RoundedSize = ( size + 15 ) >> 4;
    if ( RoundedSize < 1 ) {
        RoundedSize = 1;
    }

    for ( StCoreHeapRecord* HeapPos = m_Heap; HeapPos != NULL; HeapPos = HeapPos->Next ) {
        if ( ( HeapPos->Flags & HEAP_RECORD_FREE ) && HeapPos->Size >= RoundedSize ) {
            if ( HeapPos->Size == RoundedSize ) {
                HeapPos->Flags &= ~HEAP_RECORD_FREE;
                return HeapPos + 1;
            }

            StCoreHeapRecord* NewFreeBlock = HeapPos + ( RoundedSize + 1 );
            NewFreeBlock->Next = HeapPos->Next;
            HeapPos->Next = NewFreeBlock;
            if ( NewFreeBlock->Next != NULL ) {
                NewFreeBlock->Next->Prev = NewFreeBlock;
            }
            NewFreeBlock->Prev = HeapPos;
            NewFreeBlock->Flags = HEAP_RECORD_FREE;
            NewFreeBlock->Size = HeapPos->Size - ( RoundedSize + 1 );
            HeapPos->Size = RoundedSize;
            HeapPos->Flags &= ~HEAP_RECORD_FREE;
            return HeapPos + 1;
        }
    }
    return NULL;
}

/*
====================
CFullHeapAllocator::ReAlloc

Not supported: always fails.
====================
*/
// 0x801AC648
void* CFullHeapAllocator::ReAlloc( void* mem, int size ) {
    return NULL;
}

/*
====================
CFullHeapAllocator::Free

Marks the block free and merges it with a free neighbour on either side. Returns false for a pointer
outside the heap or a block that is already free.
====================
*/
// 0x801AC650
bool CFullHeapAllocator::Free( void* mem ) {
    if ( mem < m_Heap ) {
        return false;
    }
    if ( mem > m_HeapTop ) {
        return false;
    }

    StCoreHeapRecord* HeapPos = (StCoreHeapRecord*)mem;
    HeapPos--;
    if ( HeapPos->Flags & HEAP_RECORD_FREE ) {
        return false;
    }
    HeapPos->Flags |= HEAP_RECORD_FREE;

    StCoreHeapRecord* PrevHeap = HeapPos->Prev;
    StCoreHeapRecord* NextHeap = HeapPos->Next;

    if ( PrevHeap != NULL && ( PrevHeap->Flags & HEAP_RECORD_FREE ) ) {
        PrevHeap->Size = PrevHeap->Size + HeapPos->Size + 1;
        PrevHeap->Next = HeapPos->Next;
        if ( NextHeap != NULL ) {
            NextHeap->Prev = PrevHeap;
        }
        HeapPos = PrevHeap;
    }

    if ( NextHeap != NULL && ( NextHeap->Flags & HEAP_RECORD_FREE ) ) {
        HeapPos->Size = HeapPos->Size + NextHeap->Size + 1;
        HeapPos->Next = NextHeap->Next;
        if ( NextHeap->Next != NULL ) {
            NextHeap->Next->Prev = HeapPos;
        }
    }
    return true;
}
