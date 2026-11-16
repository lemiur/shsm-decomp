// Reference: zeldaret/tp libs/revolution/src/mem/mem_allocator.c
#include <revolution/mem.h>

/*
 * AllocatorAllocForExpHeap_
 *
 * Allocator callback: allocates from the bound expanded heap at the stored alignment.
 */
static void* AllocatorAllocForExpHeap_(MEMAllocator* allocator, u32 size) {
    return MEMAllocFromExpHeapEx(allocator->pHeap, size, allocator->heapParam1);
} // 0x80240050

/*
 * AllocatorFreeForExpHeap_
 *
 * Allocator callback: frees into the bound expanded heap.
 */
static void AllocatorFreeForExpHeap_(MEMAllocator* allocator, void* memBlock) {
    MEMFreeToExpHeap(allocator->pHeap, memBlock);
} // 0x80240060

/*
 * MEMAllocFromAllocator
 *
 * Allocates through the allocator's own alloc callback.
 */
void* MEMAllocFromAllocator(MEMAllocator* allocator, u32 size) {
    return (*allocator->pFunc->pfAlloc)(allocator, size);
} // 0x80240070

/*
 * MEMFreeToAllocator
 *
 * Frees through the allocator's own free callback.
 */
void MEMFreeToAllocator(MEMAllocator* allocator, void* memBlock) {
    (*allocator->pFunc->pfFree)(allocator, memBlock);
} // 0x80240080

/*
 * MEMInitAllocatorForExpHeap
 *
 * Binds an allocator to an expanded heap with a fixed alignment.
 */
void MEMInitAllocatorForExpHeap(MEMAllocator* allocator, MEMHeapHandle heap,
                                int align) {
    static const MEMAllocatorFunc sAllocatorFunc = {
        AllocatorAllocForExpHeap_,
        AllocatorFreeForExpHeap_
    };

    allocator->pFunc = &sAllocatorFunc;
    allocator->pHeap = heap;
    allocator->heapParam1 = align;
    allocator->heapParam2 = 0;
} // 0x80240090
