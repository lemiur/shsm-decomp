// Reference: doldecomp/ogws src/revolution/AXFX/AXFXHooks.c
#include <revolution/axfx.h>
#include <revolution/os.h>

static void* __AXFXAllocFunction(size_t size);
static void __AXFXFreeFunction(void* block);

AXFXAllocHook __AXFXAlloc = __AXFXAllocFunction;
AXFXFreeHook __AXFXFree = __AXFXFreeFunction;

/*
 * __AXFXAllocFunction / __AXFXFreeFunction
 *
 * The default effect memory hooks: the effects take their delay lines from the current OS heap.
 */
static void* __AXFXAllocFunction(size_t size) {
    return OSAllocFromHeap(__OSCurrHeap, size);
} // 0x80233470

static void __AXFXFreeFunction(void* block) {
    OSFreeToHeap(__OSCurrHeap, block);
} // 0x80233480

void AXFXSetHooks(AXFXAllocHook alloc, AXFXFreeHook free) {
    __AXFXAlloc = alloc;
    __AXFXFree = free;
} // 0x80233490

void AXFXGetHooks(AXFXAllocHook* alloc, AXFXFreeHook* free) {
    *alloc = __AXFXAlloc;
    *free = __AXFXFree;
} // 0x802334A0
