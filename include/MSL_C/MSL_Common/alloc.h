#ifndef _MSL_ALLOC_H
#define _MSL_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The system heap hooks the pool allocator takes its blocks from and returns them to. */
void* __sys_alloc(size_t size);
void __sys_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif
