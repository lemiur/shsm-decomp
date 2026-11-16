#ifndef REVOLUTION_MEM_ALLOCATOR_H
#define REVOLUTION_MEM_ALLOCATOR_H

#include <revolution/mem/heapCommon.h>
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMAllocator MEMAllocator;

typedef void* (*MEMFuncAllocatorAlloc)(MEMAllocator* allocator, u32 size);
typedef void (*MEMFuncAllocatorFree)(MEMAllocator* allocator, void* memBlock);

typedef struct MEMAllocatorFunc {
    MEMFuncAllocatorAlloc pfAlloc; // 0x00
    MEMFuncAllocatorFree pfFree;   // 0x04
} MEMAllocatorFunc;

struct MEMAllocator {
    const MEMAllocatorFunc* pFunc; // 0x00
    MEMHeapHandle pHeap;           // 0x04
    u32 heapParam1;                // 0x08: alignment
    u32 heapParam2;                // 0x0C
};

void* MEMAllocFromAllocator(MEMAllocator* allocator, u32 size);
void MEMFreeToAllocator(MEMAllocator* allocator, void* memBlock);
void MEMInitAllocatorForExpHeap(MEMAllocator* allocator, MEMHeapHandle heap,
                                int align);

#ifdef __cplusplus
}
#endif

#endif
