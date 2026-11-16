#ifndef _MSL_MEM_FUNCS_H
#define _MSL_MEM_FUNCS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Below this many bytes memmove copies byte by byte. */
#define __min_bytes_for_long_copy 32

void __copy_longs_aligned(void* dst, const void* src, unsigned long n);
void __copy_longs_rev_aligned(void* dst, const void* src, unsigned long n);
void __copy_longs_unaligned(void* dst, const void* src, unsigned long n);
void __copy_longs_rev_unaligned(void* dst, const void* src, unsigned long n);

#ifdef __cplusplus
}
#endif

#endif
