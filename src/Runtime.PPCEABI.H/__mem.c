// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/Runtime/__mem.c (memcpy, strlen)
// Reference: zeldaret/tp libs/PowerPC_EABI_Support/Runtime/Src/__mem.c (__fill_mem, memset)
//
// The Metrowerks runtime's memory primitives. memcpy, __fill_mem and memset sit in .init so the startup code can
// clear and copy sections before .text is usable; strlen is in .text.
#include <string.h>

/*
 * memcpy
 *
 * Copies n bytes from src to dst, forward or backward as the regions overlap; aligned copies move doubles after a
 * cache-block touch.
 */
// ORIGINAL-ASM: hand-written asm in the Wii runtime __mem.c (xbret/xenoblade has the same body)
__declspec(section ".init") asm void* memcpy(void* dst, const void* src, size_t n)
{
    nofralloc
    //Return if size is 0
    cmplwi cr1, r5, 0
    beqlr cr1

    cmplw cr1, r4, r3
    blt cr1, reverse
    beqlr cr1

    li r6, 0x80
    cmplw cr5, r5, r6
    blt cr5, test_word_alignment

    clrlwi r9, r4, 0x1d
    clrlwi r10, r3, 0x1d
    subf r8, r10, r3

    //Request a data cache block fetch
    dcbt 0, r4

    xor. r11, r10, r9
    bne byte_setup
    andi. r10, r10, 7
    beq+ double_copy_setup
    li r6, 8
    subf r9, r9, r6
    addi r8, r3, 0
    mtctr r9
    subf r5, r9, r5

byte_loop_double_align:
    lbz r9, 0(r4)
    addi r4, r4, 1
    stb r9, 0(r8)
    addi r8, r8, 1
    bdnz byte_loop_double_align

double_copy_setup:
    srwi r6, r5, 5
    mtctr r6

double_loop:
    lfd f1, 0(r4)
    lfd f2, 8(r4)
    lfd f3, 0x10(r4)
    lfd f4, 0x18(r4)
    addi r4, r4, 0x20
    stfd f1, 0(r8)
    stfd f2, 8(r8)
    stfd f3, 0x10(r8)
    stfd f4, 0x18(r8)

    addi r8, r8, 0x20
    bdnz double_loop
    andi. r6, r5, 0x1f
    beqlr
    addi r4, r4, -1
    mtctr r6
    addi r8, r8, -1

byte_loop_1:
    lbzu r9, 1(r4)
    stbu r9, 1(r8)
    bdnz byte_loop_1
    blr

test_word_alignment:
    li r6, 0x14
    cmplw cr5, r5, r6
    ble cr5, byte_setup
    clrlwi r9, r4, 0x1e
    clrlwi r10, r3, 0x1e
    xor. r11, r10, r9
    bne byte_setup
    li r6, 4
    subf r9, r9, r6
    addi r8, r3, 0
    subf r5, r9, r5
    mtctr r9

byte_loop_word_align:
    lbz r9, 0(r4)
    addi r4, r4, 1
    stb r9, 0(r8)
    addi r8, r8, 1
    bdnz byte_loop_word_align

word_copy_setup:
    srwi r6, r5, 4
    mtctr r6

word_loop:
    lwz r9, 0(r4)
    lwz r10, 4(r4)
    lwz r11, 8(r4)
    lwz r12, 0xc(r4)
    addi r4, r4, 0x10
    stw r9, 0(r8)
    stw r10, 4(r8)
    stw r11, 8(r8)
    stw r12, 0xc(r8)
    addi r8, r8, 0x10
    bdnz word_loop

    andi. r6, r5, 0xf
    beqlr
    addi r4, r4, -1
    mtctr r6
    addi r8, r8, -1

byte_loop_2:
    lbzu r9, 1(r4)
    stbu r9, 1(r8)
    bdnz byte_loop_2
    blr

byte_setup:
    addi r7, r4, -1
    addi r8, r3, -1
    mtctr r5

byte_loop_3:
    lbzu r9, 1(r7)
    stbu r9, 1(r8)
    bdnz byte_loop_3
    blr

reverse:
    add r4, r4, r5
    add r12, r3, r5
    li r6, 0x80
    cmplw cr5, r5, r6
    blt cr5, reverse_test_word_alignment
    clrlwi r9, r4, 0x1d
    clrlwi r10, r12, 0x1d
    xor. r11, r10, r9
    bne reverse_byte_setup
    andi. r10, r10, 7
    beq+ reverse_double_copy_setup
    mtctr r10

reverse_byte_loop_double_align:
    lbzu r9, -1(r4)
    stbu r9, -1(r12)
    bdnz reverse_byte_loop_double_align

reverse_double_copy_setup:
    subf r5, r10, r5
    srwi r6, r5, 5
    mtctr r6

reverse_double_loop:
    lfd f1, -8(r4)
    lfd f2, -0x10(r4)
    lfd f3, -0x18(r4)
    lfd f4, -0x20(r4)
    addi r4, r4, -32
    stfd f1, -8(r12)
    stfd f2, -0x10(r12)
    stfd f3, -0x18(r12)
    stfdu f4, -0x20(r12)

    bdnz reverse_double_loop

    andi. r6, r5, 0x1f
    beqlr
    mtctr r6

reverse_byte_loop_1:
    lbzu r9, -1(r4)
    stbu r9, -1(r12)
    bdnz reverse_byte_loop_1
    blr

reverse_test_word_alignment:
    li r6, 0x14
    cmplw cr5, r5, r6
    ble cr5, reverse_byte_setup
    clrlwi r9, r4, 0x1e
    clrlwi r10, r12, 0x1e
    xor. r11, r10, r9
    bne reverse_byte_setup
    andi. r10, r10, 7
    beq+ reverse_word_loop_setup
    mtctr r10

reverse_byte_loop_word_align:
    lbzu r9, -1(r4)
    stbu r9, -1(r12)
    bdnz reverse_byte_loop_word_align

reverse_word_loop_setup:
    subf r5, r10, r5
    srwi r6, r5, 4
    mtctr r6

reverse_word_loop:
    lwz r9, -4(r4)
    lwz r10, -8(r4)
    lwz r11, -0xc(r4)
    lwz r8, -0x10(r4)
    addi r4, r4, -16
    stw r9, -4(r12)
    stw r10, -8(r12)
    stw r11, -0xc(r12)
    stwu r8, -0x10(r12)
    bdnz reverse_word_loop

    andi. r6, r5, 0xf
    beqlr
    mtctr r6

reverse_byte_loop_2:
    lbzu r9, -1(r4)
    stbu r9, -1(r12)
    bdnz reverse_byte_loop_2
    blr

reverse_byte_setup:
    mtctr r5

reverse_byte_loop_3:
    lbzu r9, -1(r4)
    stbu r9, -1(r12)
    bdnz reverse_byte_loop_3
    blr
} // 0x80005F34

/*
 * __fill_mem
 *
 * Fills n bytes at dst with val: bytes up to word alignment, then 32-byte blocks, then words, then the tail.
 */
__declspec(section ".init") void __fill_mem(void* dst, int val, size_t n)
{
    unsigned long v = (unsigned char)val;
    unsigned long i;

    ((unsigned char*)dst) = ((unsigned char*)dst) - 1;

    if (n >= 32) {
        i = (~(unsigned long)dst) & 3;

        if (i) {
            n -= i;

            do {
                *++(((unsigned char*)dst)) = v;
            } while (--i);
        }

        if (v) {
            v |= v << 24 | v << 16 | v << 8;
        }

        ((unsigned long*)dst) = ((unsigned long*)(((unsigned char*)dst) + 1)) - 1;

        i = n >> 5;

        if (i) {
            do {
                *++((unsigned long*)dst) = v;
                *++((unsigned long*)dst) = v;
                *++((unsigned long*)dst) = v;
                *++((unsigned long*)dst) = v;
                *++((unsigned long*)dst) = v;
                *++((unsigned long*)dst) = v;
                *++((unsigned long*)dst) = v;
                *++((unsigned long*)dst) = v;
            } while (--i);
        }

        i = (n & 31) >> 2;

        if (i) {
            do {
                *++((unsigned long*)dst) = v;
            } while (--i);
        }

        ((unsigned char*)dst) = ((unsigned char*)(((unsigned long*)dst) + 1)) - 1;

        n &= 3;
    }

    if (n) {
        do {
            *++((unsigned char*)dst) = v;
        } while (--n);
    }
} // 0x800061D0

/*
 * memset
 *
 * Fills n bytes at dst with val and returns dst.
 */
__declspec(section ".init") void* memset(void* dst, int val, size_t n)
{
    __fill_mem(dst, val, n);

    return dst;
} // 0x80006284

/*
 * strlen
 *
 * Returns the length of str.
 */
size_t strlen(const char* str)
{
    size_t len = -1;
    unsigned char* p = (unsigned char*)str - 1;

    do {
        len++;
    } while (*++p);

    return len;
} // 0x801A622C
