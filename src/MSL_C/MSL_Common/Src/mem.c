#include <string.h>
#include <MSL_C/MSL_Common/mem_funcs.h>

/*
 * memmove
 *
 * Copies forwards unless the destination overlaps the end of the source.
 * Long copies go word by word once dst is word aligned.
 */
void* memmove(void* dst, const void* src, size_t n) {
    const char* p;
    char* q;
    int rev = ((unsigned long)src < (unsigned long)dst);

    if (n >= __min_bytes_for_long_copy) {
        if (((int)dst ^ (int)src) & 3) {
            if (!rev) {
                __copy_longs_unaligned(dst, src, n);
            } else {
                __copy_longs_rev_unaligned(dst, src, n);
            }
        } else {
            if (!rev) {
                __copy_longs_aligned(dst, src, n);
            } else {
                __copy_longs_rev_aligned(dst, src, n);
            }
        }

        return dst;
    }

    if (!rev) {
        for (p = (const char*)src - 1, q = (char*)dst - 1, n++; --n;) {
            *++q = *++p;
        }
    } else {
        for (p = (const char*)src + n, q = (char*)dst + n, n++; --n;) {
            *--q = *--p;
        }
    }

    return dst;
} // 0x80199540

void* memchr(const void* src, int val, size_t n) {
    const unsigned char* p;
    unsigned long v = (val & 0xFF);

    for (p = (const unsigned char*)src - 1, n++; --n;) {
        if ((*++p & 0xFF) == v) {
            return (void*)p;
        }
    }

    return NULL;
} // 0x8019960C

/*
 * __memrchr
 *
 * memchr searching backwards from the end of the block.
 */
void* __memrchr(const void* src, int val, size_t n) {
    const unsigned char* p;
    unsigned long v = (val & 0xFF);

    for (p = (const unsigned char*)src + n, n++; --n;) {
        if (*--p == v) {
            return (void*)p;
        }
    }

    return NULL;
} // 0x80199638

int memcmp(const void* src1, const void* src2, size_t n) {
    const unsigned char* p1;
    const unsigned char* p2;

    for (p1 = (const unsigned char*)src1 - 1, p2 = (const unsigned char*)src2 - 1, n++; --n;) {
        if (*++p1 != *++p2) {
            return ((*p1 < *p2) ? -1 : +1);
        }
    }

    return 0;
} // 0x80199664
