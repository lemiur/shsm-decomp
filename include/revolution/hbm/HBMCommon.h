#ifndef HBM_COMMON_H
#define HBM_COMMON_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMCommon.h

#include <revolution/types.h>
#include <revolution/mem/allocator.h>

/* The allocator the menu takes its memory from (set up by HBMCreate). */
extern "C" MEMAllocator* spAllocator;

void* HBMAllocMem(u32 size);
void HBMFreeMem(void* mem);

#endif
