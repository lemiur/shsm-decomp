#ifndef REVOLUTION_OS_OSALLOC_H
#define REVOLUTION_OS_OSALLOC_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int OSHeapHandle;

/* Header of a heap block; lists are kept in address order when free. A block handed out by OSAllocFromHeap
 * begins OSRoundUp32B(sizeof(Cell)) bytes after its Cell (FMOD's realloc reads the size from it). */
typedef struct Cell {
    struct Cell* prev;
    struct Cell* next;
    long size;              /* bytes, including this header */
} Cell;

extern volatile OSHeapHandle __OSCurrHeap; // volatile: declared so by the SDK header

void* OSAllocFromHeap(OSHeapHandle heap, u32 size);
void OSFreeToHeap(OSHeapHandle heap, void* ptr);
OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap);
void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps);
OSHeapHandle OSCreateHeap(void* start, void* end);

#define OSAlloc(size) OSAllocFromHeap(__OSCurrHeap, (size))
#define OSFree(ptr) OSFreeToHeap(__OSCurrHeap, (ptr))

#ifdef __cplusplus
}
#endif

#endif
