#include <PowerPC_EABI_Support/Runtime/runtime.h>

/*
 * Metrowerks runtime support: 64-bit integer arithmetic and conversions, the
 * register save/restore helpers used by compiled prologues, and the
 * double-to-unsigned conversion. Every routine is hand-written assembly in the
 * original (nofralloc, operands fixed by the EABI), and the _savegpr_N family
 * are extra entry points into their routines.
 */

/* Bounds used by __cvt_fp2unsigned: 0, 2^32, 2^31. */
static const double __cvt_fp2unsigned_limits[3] = {0.0, 4294967296.0, 2147483648.0};

asm unsigned long __cvt_fp2unsigned(register double d) {
    nofralloc
    stwu r1, -0x10(r1)
    lis r4, __cvt_fp2unsigned_limits@ha
    addi r4, r4, __cvt_fp2unsigned_limits@l
    li r3, 0x0
    lfd f0, 0x0(r4)
    lfd f3, 0x8(r4)
    lfd f4, 0x10(r4)
    fcmpu cr0, f1, f0
    fcmpu cr6, f1, f3
    blt L2
    subi r3, r3, 0x1
    bge cr6, L2
    fcmpu cr7, f1, f4
    fmr f2, f1
    blt cr7, L1
    fsub f2, f1, f4
L1:
    fctiwz f2, f2
    stfd f2, 0x8(r1)
    lwz r3, 0xc(r1)
    blt cr7, L2
    addis r3, r3, 0x8000
L2:
    addi r1, r1, 0x10
    blr
}

asm void __save_fpr(void) {
    nofralloc
    entry _savefpr_14
    stfd f14, -0x90(r11)
    entry _savefpr_15
    stfd f15, -0x88(r11)
    entry _savefpr_16
    stfd f16, -0x80(r11)
    entry _savefpr_17
    stfd f17, -0x78(r11)
    entry _savefpr_18
    stfd f18, -0x70(r11)
    entry _savefpr_19
    stfd f19, -0x68(r11)
    entry _savefpr_20
    stfd f20, -0x60(r11)
    entry _savefpr_21
    stfd f21, -0x58(r11)
    entry _savefpr_22
    stfd f22, -0x50(r11)
    entry _savefpr_23
    stfd f23, -0x48(r11)
    entry _savefpr_24
    stfd f24, -0x40(r11)
    entry _savefpr_25
    stfd f25, -0x38(r11)
    entry _savefpr_26
    stfd f26, -0x30(r11)
    entry _savefpr_27
    stfd f27, -0x28(r11)
    entry _savefpr_28
    stfd f28, -0x20(r11)
    entry _savefpr_29
    stfd f29, -0x18(r11)
    entry _savefpr_30
    stfd f30, -0x10(r11)
    entry _savefpr_31
    stfd f31, -0x8(r11)
    blr
}

asm void __restore_fpr(void) {
    nofralloc
    entry _restfpr_14
    lfd f14, -0x90(r11)
    entry _restfpr_15
    lfd f15, -0x88(r11)
    entry _restfpr_16
    lfd f16, -0x80(r11)
    entry _restfpr_17
    lfd f17, -0x78(r11)
    entry _restfpr_18
    lfd f18, -0x70(r11)
    entry _restfpr_19
    lfd f19, -0x68(r11)
    entry _restfpr_20
    lfd f20, -0x60(r11)
    entry _restfpr_21
    lfd f21, -0x58(r11)
    entry _restfpr_22
    lfd f22, -0x50(r11)
    entry _restfpr_23
    lfd f23, -0x48(r11)
    entry _restfpr_24
    lfd f24, -0x40(r11)
    entry _restfpr_25
    lfd f25, -0x38(r11)
    entry _restfpr_26
    lfd f26, -0x30(r11)
    entry _restfpr_27
    lfd f27, -0x28(r11)
    entry _restfpr_28
    lfd f28, -0x20(r11)
    entry _restfpr_29
    lfd f29, -0x18(r11)
    entry _restfpr_30
    lfd f30, -0x10(r11)
    entry _restfpr_31
    lfd f31, -0x8(r11)
    blr
}

asm void __save_gpr(void) {
    nofralloc
    entry _savegpr_14
    stw r14, -0x48(r11)
    entry _savegpr_15
    stw r15, -0x44(r11)
    entry _savegpr_16
    stw r16, -0x40(r11)
    entry _savegpr_17
    stw r17, -0x3c(r11)
    entry _savegpr_18
    stw r18, -0x38(r11)
    entry _savegpr_19
    stw r19, -0x34(r11)
    entry _savegpr_20
    stw r20, -0x30(r11)
    entry _savegpr_21
    stw r21, -0x2c(r11)
    entry _savegpr_22
    stw r22, -0x28(r11)
    entry _savegpr_23
    stw r23, -0x24(r11)
    entry _savegpr_24
    stw r24, -0x20(r11)
    entry _savegpr_25
    stw r25, -0x1c(r11)
    entry _savegpr_26
    stw r26, -0x18(r11)
    entry _savegpr_27
    stw r27, -0x14(r11)
    entry _savegpr_28
    stw r28, -0x10(r11)
    entry _savegpr_29
    stw r29, -0xc(r11)
    entry _savegpr_30
    stw r30, -0x8(r11)
    entry _savegpr_31
    stw r31, -0x4(r11)
    blr
}

asm void __restore_gpr(void) {
    nofralloc
    entry _restgpr_14
    lwz r14, -0x48(r11)
    entry _restgpr_15
    lwz r15, -0x44(r11)
    entry _restgpr_16
    lwz r16, -0x40(r11)
    entry _restgpr_17
    lwz r17, -0x3c(r11)
    entry _restgpr_18
    lwz r18, -0x38(r11)
    entry _restgpr_19
    lwz r19, -0x34(r11)
    entry _restgpr_20
    lwz r20, -0x30(r11)
    entry _restgpr_21
    lwz r21, -0x2c(r11)
    entry _restgpr_22
    lwz r22, -0x28(r11)
    entry _restgpr_23
    lwz r23, -0x24(r11)
    entry _restgpr_24
    lwz r24, -0x20(r11)
    entry _restgpr_25
    lwz r25, -0x1c(r11)
    entry _restgpr_26
    lwz r26, -0x18(r11)
    entry _restgpr_27
    lwz r27, -0x14(r11)
    entry _restgpr_28
    lwz r28, -0x10(r11)
    entry _restgpr_29
    lwz r29, -0xc(r11)
    entry _restgpr_30
    lwz r30, -0x8(r11)
    entry _restgpr_31
    lwz r31, -0x4(r11)
    blr
}

asm unsigned long long __div2u(register unsigned long long dividend, register unsigned long long divisor) {
    nofralloc
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne L1
    addi r0, r9, 0x20
L1:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne L2
    addi r9, r10, 0x20
L2:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt L9
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt L3
    srw r8, r3, r7
    li r7, 0x0
    b L4
L3:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
L4:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt L5
    slw r3, r4, r9
    li r4, 0x0
    b L6
L5:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
L6:
    li r10, -0x1
    addic r7, r7, 0x0
L7:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt L8
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
L8:
    bdnz L7
    adde r4, r4, r4
    adde r3, r3, r3
    blr
L9:
    li r4, 0x0
    li r3, 0x0
    blr
}

asm long long __div2i(register long long dividend, register long long divisor) {
    nofralloc
    stwu r1, -0x10(r1)
    clrrwi. r9, r3, 31
    beq L1
    subfic r4, r4, 0x0
    subfze r3, r3
L1:
    stw r9, 0x8(r1)
    clrrwi. r10, r5, 31
    beq L2
    subfic r6, r6, 0x0
    subfze r5, r5
L2:
    stw r10, 0xc(r1)
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne L3
    addi r0, r9, 0x20
L3:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne L4
    addi r9, r10, 0x20
L4:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt L12
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt L5
    srw r8, r3, r7
    li r7, 0x0
    b L6
L5:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
L6:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt L7
    slw r3, r4, r9
    li r4, 0x0
    b L8
L7:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
L8:
    li r10, -0x1
    addic r7, r7, 0x0
L9:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt L10
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
L10:
    bdnz L9
    adde r4, r4, r4
    adde r3, r3, r3
    lwz r9, 0x8(r1)
    lwz r10, 0xc(r1)
    xor. r7, r9, r10
    beq L11
    cmpwi r9, 0x0
    subfic r4, r4, 0x0
    subfze r3, r3
L11:
    b L13
L12:
    li r4, 0x0
    li r3, 0x0
L13:
    addi r1, r1, 0x10
    blr
}

asm unsigned long long __mod2u(register unsigned long long dividend, register unsigned long long divisor) {
    nofralloc
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne L1
    addi r0, r9, 0x20
L1:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne L2
    addi r9, r10, 0x20
L2:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt L9
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt L3
    srw r8, r3, r7
    li r7, 0x0
    b L4
L3:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
L4:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt L5
    slw r3, r4, r9
    li r4, 0x0
    b L6
L5:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
L6:
    li r10, -0x1
    addic r7, r7, 0x0
L7:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt L8
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
L8:
    bdnz L7
    mr r4, r8
    mr r3, r7
    blr
L9:
    blr
}

asm long long __mod2i(register long long dividend, register long long divisor) {
    nofralloc
    cmpwi cr7, r3, 0x0
    bge cr7, L1
    subfic r4, r4, 0x0
    subfze r3, r3
L1:
    cmpwi r5, 0x0
    bge L2
    subfic r6, r6, 0x0
    subfze r5, r5
L2:
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne L3
    addi r0, r9, 0x20
L3:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne L4
    addi r9, r10, 0x20
L4:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt L11
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt L5
    srw r8, r3, r7
    li r7, 0x0
    b L6
L5:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
L6:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt L7
    slw r3, r4, r9
    li r4, 0x0
    b L8
L7:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
L8:
    li r10, -0x1
    addic r7, r7, 0x0
L9:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt L10
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
L10:
    bdnz L9
    mr r4, r8
    mr r3, r7
L11:
    bge cr7, L12
    subfic r4, r4, 0x0
    subfze r3, r3
L12:
    blr
}

asm long long __shl2i(register long long value, register int count) {
    nofralloc
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    slw r3, r3, r5
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r5
    blr
}

asm float __cvt_sll_flt(register long long value) {
    nofralloc
    stwu r1, -0x10(r1)
    clrrwi. r5, r3, 31
    beq L1
    subfic r4, r4, 0x0
    subfze r3, r3
L1:
    or. r7, r3, r4
    li r6, 0x0
    beq L4
    cntlzw r7, r3
    cntlzw r8, r4
    extlwi r9, r7, 5, 26
    srawi r9, r9, 31
    and r9, r9, r8
    add r7, r7, r9
    subfic r8, r7, 0x20
    subic r9, r7, 0x20
    slw r3, r3, r7
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r7
    subf r6, r7, r6
    clrlwi r7, r4, 21
    cmpwi r7, 0x400
    addi r6, r6, 0x43e
    blt L3
    bgt L2
    rlwinm. r7, r4, 0, 20, 20
    beq L3
L2:
    addic r4, r4, 0x800
    addze r3, r3
    addze r6, r6
L3:
    rotrwi r4, r4, 11
    rlwimi r4, r3, 21, 0, 10
    extrwi r3, r3, 20, 1
    slwi r6, r6, 20
    or r3, r6, r3
    or r3, r5, r3
L4:
    stw r3, 0x8(r1)
    stw r4, 0xc(r1)
    lfd f1, 0x8(r1)
    frsp f1, f1
    addi r1, r1, 0x10
    blr
}

asm float __cvt_ull_flt(register unsigned long long value) {
    nofralloc
    stwu r1, -0x10(r1)
    or. r7, r3, r4
    li r6, 0x0
    beq L3
    cntlzw r7, r3
    cntlzw r8, r4
    extlwi r9, r7, 5, 26
    srawi r9, r9, 31
    and r9, r9, r8
    add r7, r7, r9
    subfic r8, r7, 0x20
    subic r9, r7, 0x20
    slw r3, r3, r7
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r7
    subf r6, r7, r6
    clrlwi r7, r4, 21
    cmpwi r7, 0x400
    addi r6, r6, 0x43e
    blt L2
    bgt L1
    rlwinm. r7, r4, 0, 20, 20
    beq L2
L1:
    addic r4, r4, 0x800
    addze r3, r3
    addze r6, r6
L2:
    rotrwi r4, r4, 11
    rlwimi r4, r3, 21, 0, 10
    extrwi r3, r3, 20, 1
    slwi r6, r6, 20
    or r3, r6, r3
L3:
    stw r3, 0x8(r1)
    stw r4, 0xc(r1)
    lfd f1, 0x8(r1)
    frsp f1, f1
    addi r1, r1, 0x10
    blr
}

asm long long __cvt_dbl_usll(register double value) {
    nofralloc
    stwu r1, -0x10(r1)
    stfd f1, 0x8(r1)
    lwz r3, 0x8(r1)
    lwz r4, 0xc(r1)
    extrwi r5, r3, 11, 1
    cmplwi r5, 0x3ff
    bge L1
    li r3, 0x0
    li r4, 0x0
    b L6
L1:
    mr r6, r3
    clrlwi r3, r3, 12
    oris r3, r3, 0x10
    subi r5, r5, 0x433
    cmpwi r5, 0x0
    bge L2
    neg r5, r5
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    srw r4, r4, r5
    slw r10, r3, r8
    or r4, r4, r10
    srw r10, r3, r9
    or r4, r4, r10
    srw r3, r3, r5
    b L5
L2:
    cmpwi r5, 0xa
    ble+ L4
    clrrwi. r6, r6, 31
    beq L3
    lis r3, 0x8000
    li r4, 0x0
    b L6
L3:
    lis r3, 0x7fff
    ori r3, r3, 0xffff
    li r4, -0x1
    b L6
L4:
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    slw r3, r3, r5
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r5
L5:
    clrrwi. r6, r6, 31
    beq L6
    subfic r4, r4, 0x0
    subfze r3, r3
L6:
    addi r1, r1, 0x10
    blr
}

asm unsigned long long __cvt_dbl_ull(register double value) {
    nofralloc
    stwu r1, -0x10(r1)
    stfd f1, 0x8(r1)
    lwz r3, 0x8(r1)
    lwz r4, 0xc(r1)
    extrwi r5, r3, 11, 1
    cmplwi r5, 0x3ff
    bge L2
L1:
    li r3, 0x0
    li r4, 0x0
    b L5
L2:
    clrrwi. r6, r3, 31
    bne L1
    clrlwi r3, r3, 12
    oris r3, r3, 0x10
    subi r5, r5, 0x433
    cmpwi r5, 0x0
    bge L3
    neg r5, r5
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    srw r4, r4, r5
    slw r10, r3, r8
    or r4, r4, r10
    srw r10, r3, r9
    or r4, r4, r10
    srw r3, r3, r5
    b L5
L3:
    cmpwi r5, 0xb
    ble+ L4
    li r3, -0x1
    li r4, -0x1
    b L5
L4:
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    slw r3, r3, r5
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r5
L5:
    addi r1, r1, 0x10
    blr
}
