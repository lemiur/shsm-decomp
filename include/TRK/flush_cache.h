// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/ppc/Generic/flush_cache.h

#ifndef TRK_FLUSH_CACHE_H
#define TRK_FLUSH_CACHE_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Writes back the data cache and invalidates the instruction cache for size bytes starting at addr. */
void TRK_flush_cache(u32 addr, int size);

#ifdef __cplusplus
}
#endif

#endif /* TRK_FLUSH_CACHE_H */
