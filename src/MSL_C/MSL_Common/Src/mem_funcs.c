// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common/Src/mem_funcs.c
#include <MSL_C/MSL_Common/mem_funcs.h>

/*
 * Word-at-a-time copy loops behind memmove. The forward copies walk
 * pre-incremented pointers, so they start one element before the data.
 */

#define cps ((unsigned char*)src)
#define cpd ((unsigned char*)dst)
#define lps ((unsigned long*)src)
#define lpd ((unsigned long*)dst)

#define deref_auto_inc(p) *++(p)

/*
 * __copy_longs_aligned
 *
 * Forward copy for src and dst with the same alignment within a word.
 */
void __copy_longs_aligned(void* dst, const void* src, unsigned long n) {
    unsigned long i;

    i = (-(unsigned long)dst) & 3;

    cps = ((unsigned char*)src) - 1;
    cpd = ((unsigned char*)dst) - 1;

    if (i) {
        n -= i;
        do {
            deref_auto_inc(cpd) = deref_auto_inc(cps);
        } while (--i);
    }

    lps = ((unsigned long*)(cps + 1)) - 1;
    lpd = ((unsigned long*)(cpd + 1)) - 1;

    i = n >> 5;

    if (i) {
        do {
            deref_auto_inc(lpd) = deref_auto_inc(lps);
            deref_auto_inc(lpd) = deref_auto_inc(lps);
            deref_auto_inc(lpd) = deref_auto_inc(lps);
            deref_auto_inc(lpd) = deref_auto_inc(lps);
            deref_auto_inc(lpd) = deref_auto_inc(lps);
            deref_auto_inc(lpd) = deref_auto_inc(lps);
            deref_auto_inc(lpd) = deref_auto_inc(lps);
            deref_auto_inc(lpd) = deref_auto_inc(lps);
        } while (--i);
    }

    i = (n & 31) >> 2;

    if (i) {
        do {
            deref_auto_inc(lpd) = deref_auto_inc(lps);
        } while (--i);
    }

    cps = ((unsigned char*)(lps + 1)) - 1;
    cpd = ((unsigned char*)(lpd + 1)) - 1;

    n &= 3;

    if (n) {
        do {
            deref_auto_inc(cpd) = deref_auto_inc(cps);
        } while (--n);
    }

    return;
} // 0x801996B0

/*
 * __copy_longs_rev_aligned
 *
 * Backward copy for src and dst with the same alignment within a word.
 */
void __copy_longs_rev_aligned(void* dst, const void* src, unsigned long n) {
    unsigned long i;

    cps = ((unsigned char*)src) + n;
    cpd = ((unsigned char*)dst) + n;

    i = ((unsigned long)cpd) & 3;

    if (i) {
        n -= i;
        do {
            *--cpd = *--cps;
        } while (--i);
    }

    i = n >> 5;

    if (i) {
        do {
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
            *--lpd = *--lps;
        } while (--i);
    }

    i = (n & 31) >> 2;

    if (i) {
        do {
            *--lpd = *--lps;
        } while (--i);
    }

    n &= 3;

    if (n) {
        do {
            *--cpd = *--cps;
        } while (--n);
    }

    return;
} // 0x8019976C

/*
 * __copy_longs_unaligned
 *
 * Forward copy for src and dst with different alignments: aligns dst,
 * then builds each destination word from two source words.
 */
void __copy_longs_unaligned(void* dst, const void* src, unsigned long n) {
    unsigned long i, v1, v2;
    unsigned int src_offset, left_shift, right_shift;

    i = (-(unsigned long)dst) & 3;

    cps = ((unsigned char*)src) - 1;
    cpd = ((unsigned char*)dst) - 1;

    if (i) {
        n -= i;
        do {
            deref_auto_inc(cpd) = deref_auto_inc(cps);
        } while (--i);
    }

    src_offset = ((unsigned int)(cps + 1)) & 3;

    left_shift = src_offset << 3;
    right_shift = 32 - left_shift;

    cps -= src_offset;

    lps = ((unsigned long*)(cps + 1)) - 1;
    lpd = ((unsigned long*)(cpd + 1)) - 1;

    i = n >> 3;

    v1 = deref_auto_inc(lps);

    do {
        v2 = deref_auto_inc(lps);
        deref_auto_inc(lpd) = (v1 << left_shift) | (v2 >> right_shift);
        v1 = deref_auto_inc(lps);
        deref_auto_inc(lpd) = (v2 << left_shift) | (v1 >> right_shift);
    } while (--i);

    if (n & 4) {
        v2 = deref_auto_inc(lps);
        deref_auto_inc(lpd) = (v1 << left_shift) | (v2 >> right_shift);
    }

    cps = ((unsigned char*)(lps + 1)) - 1;
    cpd = ((unsigned char*)(lpd + 1)) - 1;

    n &= 3;

    if (n) {
        cps -= 4 - src_offset;
        do {
            deref_auto_inc(cpd) = deref_auto_inc(cps);
        } while (--n);
    }

    return;
} // 0x80199814

/*
 * __copy_longs_rev_unaligned
 *
 * Backward counterpart of __copy_longs_unaligned.
 *
 * The two `cps = (unsigned char*)lps; cpd = (unsigned char*)lpd;` pairs are
 * the Wii-build lines of the reference (guarded there by !PLATFORM_GCN). They
 * generate no code of their own, but the lvalue-cast self-assignment keeps the
 * entry copy of src, which fixes the register numbering.
 */
void __copy_longs_rev_unaligned(void* dst, const void* src, unsigned long n) {
    unsigned long i, v1, v2;
    unsigned int src_offset, left_shift, right_shift;

    cps = ((unsigned char*)src) + n;
    cpd = ((unsigned char*)dst) + n;

    i = ((unsigned long)cpd) & 3;

    if (i) {
        n -= i;
        do {
            *--cpd = *--cps;
        } while (--i);
    }

    src_offset = ((unsigned int)cps) & 3;

    left_shift = src_offset << 3;
    right_shift = 32 - left_shift;

    cps += 4 - src_offset;

    cps = ((unsigned char*)lps);
    cpd = ((unsigned char*)lpd);

    i = n >> 3;

    v1 = *--lps;

    do {
        v2 = *--lps;
        *--lpd = (v2 << left_shift) | (v1 >> right_shift);
        v1 = *--lps;
        *--lpd = (v1 << left_shift) | (v2 >> right_shift);
    } while (--i);

    if (n & 4) {
        v2 = *--lps;
        *--lpd = (v2 << left_shift) | (v1 >> right_shift);
    }

    n &= 3;

    cps = ((unsigned char*)lps);
    cpd = ((unsigned char*)lpd);

    if (n) {
        cps += src_offset;
        do {
            *--cpd = *--cps;
        } while (--n);
    }

    return;
} // 0x801998D4
