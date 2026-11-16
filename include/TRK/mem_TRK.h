#ifndef TRK_MEM_TRK_H
#define TRK_MEM_TRK_H

#include <stddef.h>
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Byte-wise copy and fill through aligned word accesses (read-modify-write of the word that holds each
 * byte), safe on memory that takes only word accesses. Both return dst. */
void* TRK_memcpy(void* dst, const void* src, int size);
void TRK_fill_mem(u8* dst, int val, int size);
void* TRK_memset(void* dst, int val, int size);

#ifdef __cplusplus
}
#endif

#endif /* TRK_MEM_TRK_H */
