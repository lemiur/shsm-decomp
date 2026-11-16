// Reference: doldecomp/ogws include/revolution/AXFX/AXFXHooks.h
#ifndef REVOLUTION_AXFX_HOOKS_H
#define REVOLUTION_AXFX_HOOKS_H

#include <revolution/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*AXFXAllocHook)(size_t size);
typedef void (*AXFXFreeHook)(void* block);

extern AXFXAllocHook __AXFXAlloc;
extern AXFXFreeHook __AXFXFree;

void AXFXSetHooks(AXFXAllocHook alloc, AXFXFreeHook free);
void AXFXGetHooks(AXFXAllocHook* alloc, AXFXFreeHook* free);

#ifdef __cplusplus
}
#endif

#endif
