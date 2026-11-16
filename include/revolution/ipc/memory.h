// Reference: zeldaret/tp libs/revolution/include/revolution/private/iostypes.h
//
// IOS heap: the allocator over the IPC shared-memory buffers.
#ifndef REVOLUTION_IPC_MEMORY_H
#define REVOLUTION_IPC_MEMORY_H

#include <revolution/types.h>
#include <revolution/ios/iostypes.h>

#ifdef __cplusplus
extern "C" {
#endif

IOSHeapId iosCreateHeap(void* ptr, u32 size);
void* __iosAlloc(IOSHeapId id, u32 size, u32 alignment);
void* iosAllocAligned(IOSHeapId id, u32 size, u32 alignment);
IOSError iosFree(IOSHeapId id, void* ptr);

#ifdef __cplusplus
}
#endif

#endif
