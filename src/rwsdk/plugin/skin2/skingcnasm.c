// Unit name and boundary: Gormiti link map (RpSkin2.a skingcnasm.obj; its source there is
// gcn/skingcnasm.c, whose six functions are hand-written assembly).
// Function names, parameters and line table: Gormiti DWARF (Final and Debug builds).
/*
 * skingcnasm.c: RpSkin2's vertex skinning loops for the GameCube, written in assembly. One
 * function per case of the skin's weights per vertex (two, three or four) and of the arrays
 * skinned (positions only, "P", or positions and normals, "PN").
 *
 * Each function takes the matrix palette (r3, 64 bytes per RwMatrix, indexed by the bone indices
 * in the skin's platform data), the skin (r4: platformData.weights at 0x24 and platformData.indices
 * at 0x28) and the job (r5, a SkinAsmData) and has no result. For every vertex
 *
 *     dst = w0 * (M[b0] * src) + w1 * (M[b1] * src) + ...
 *
 * where positions are transformed as points and normals by the rotation part only (no
 * renormalisation). The first influence is always applied; the next ones only while their weight
 * is positive (a zero weight ends the sum). Components
 * move through the graphics quantisation registers: GQR0 is float (matrices), GQR5 the unsigned
 * byte weights, GQR6 the source and destination positions, GQR7 the normals. The loops are
 * software pipelined: the next vertex is loaded while the previous one is stored.
 *
 * The array pointers are kept one component (posSize or nrmSize) before their start, for the
 * update forms psq_lux/psq_stux; each vertex is x,y through the first index register, z through
 * the second.
 *
 * Parameter registers: r3 matrixArray, r4 skin, r5 data. In the body r4 and r5 are reused as the
 * running weight and bone index pointers.
 */
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plugin/skin2/rpskin.h>

/*
 * ==================== _rwDlSkinUpdate2WeightsP ====================
 * Skins the positions of vertices with two weights: per vertex one halfword of indices (the
 * two bone bytes) and two weight bytes.
 */
// ORIGINAL-ASM: hand-written assembly (frame, paired-single schedule, one source line per instruction in the Gormiti line table)
asm void _rwDlSkinUpdate2WeightsP(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data) {
    nofralloc
    stwu r1, -0x18(r1)
    stfd f30, 0x8(r1)
    stfd f31, 0x10(r1)
    lwz r0, 0x1c(r5)
    cmpwi r0, 0x0
    mtctr r0
    ble done
    lwz r6, 0x0(r5)
    lwz r7, 0x8(r5)
    lwz r8, 0x10(r5)
    lwz r5, 0x28(r4)
    lwz r4, 0x24(r4)
    rotlwi r9, r8, 1
    subf r6, r8, r6
    subf r7, r8, r7
    ps_sub f30, f30, f30
    subi r5, r5, 0x2
    subi r4, r4, 0x2
    lhzu r0, 0x2(r5)
    psq_lux f8, r7, r8, 0, 6
    psq_lux f9, r7, r9, 1, 6
vertexLoop:
    rlwinm r10, r0, 30, 18, 25
    add r10, r10, r3
    psq_l f0, 0x0(r10), 0, 0
    psq_l f1, 0x8(r10), 1, 0
    psq_l f6, 0x30(r10), 0, 0
    psq_l f7, 0x38(r10), 1, 0
    psq_l f2, 0x10(r10), 0, 0
    psq_l f3, 0x18(r10), 1, 0
    ps_madds0 f10, f0, f8, f6
    ps_madds0 f11, f1, f8, f7
    psq_l f4, 0x20(r10), 0, 0
    psq_l f5, 0x28(r10), 1, 0
    ps_madds1 f10, f2, f8, f10
    ps_madds1 f11, f3, f8, f11
    psq_lu f31, 0x2(r4), 0, 5
    ps_madds0 f10, f4, f9, f10
    ps_madds0 f11, f5, f9, f11
    ps_cmpo1 cr5, f31, f30
    ps_muls0 f10, f10, f31
    ps_muls0 f11, f11, f31
    ble cr5, storeVertex
    clrlslwi r10, r0, 24, 6
    add r10, r10, r3
    psq_l f0, 0x0(r10), 0, 0
    psq_l f1, 0x8(r10), 1, 0
    psq_l f6, 0x30(r10), 0, 0
    psq_l f7, 0x38(r10), 1, 0
    psq_l f2, 0x10(r10), 0, 0
    psq_l f3, 0x18(r10), 1, 0
    ps_madds0 f12, f0, f8, f6
    ps_madds0 f13, f1, f8, f7
    psq_l f4, 0x20(r10), 0, 0
    psq_l f5, 0x28(r10), 1, 0
    ps_madds1 f12, f2, f8, f12
    ps_madds1 f13, f3, f8, f13
    ps_madds0 f12, f4, f9, f12
    ps_madds0 f13, f5, f9, f13
    ps_madds1 f10, f12, f31, f10
    ps_madds1 f11, f13, f31, f11
storeVertex:
    lhzu r0, 0x2(r5)
    psq_lux f8, r7, r8, 0, 6
    psq_lux f9, r7, r9, 1, 6
    psq_stux f10, r6, r8, 0, 6
    psq_stux f11, r6, r9, 1, 6
    bdnz vertexLoop
done:
    lfd f30, 0x8(r1)
    lfd f31, 0x10(r1)
    addi r1, r1, 0x18
    blr
} // 0x802ECB8C

/*
 * ==================== _rwDlSkinUpdate2WeightsPN ====================
 * Skins the positions and normals of vertices with two weights: per vertex one halfword of
 * indices and two weight bytes. The normals take the rotation only and skip nrmSkip bytes after
 * each vertex in both arrays.
 */
// ORIGINAL-ASM: hand-written assembly (frame, paired-single schedule, one source line per instruction in the Gormiti line table)
asm void _rwDlSkinUpdate2WeightsPN(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data) {
    nofralloc
    stwu r1, -0x58(r1)
    stw r29, 0x8(r1)
    stw r30, 0xc(r1)
    stw r31, 0x10(r1)
    stfd f24, 0x18(r1)
    stfd f25, 0x20(r1)
    stfd f26, 0x28(r1)
    stfd f27, 0x30(r1)
    stfd f28, 0x38(r1)
    stfd f29, 0x40(r1)
    stfd f30, 0x48(r1)
    stfd f31, 0x50(r1)
    lwz r0, 0x1c(r5)
    cmpwi r0, 0x0
    mtctr r0
    ble done
    lwz r6, 0x0(r5)
    lwz r7, 0x4(r5)
    lwz r8, 0x8(r5)
    lwz r9, 0xc(r5)
    lwz r10, 0x10(r5)
    lwz r11, 0x14(r5)
    lwz r12, 0x18(r5)
    lwz r5, 0x28(r4)
    lwz r4, 0x24(r4)
    rotlwi r30, r10, 1
    rotlwi r31, r11, 1
    subi r5, r5, 0x2
    subi r4, r4, 0x2
    subf r6, r10, r6
    subf r7, r11, r7
    subf r8, r10, r8
    subf r9, r11, r9
    ps_sub f13, f13, f13
    lhzu r0, 0x2(r5)
    psq_lux f8, r8, r10, 0, 6
    psq_lux f9, r8, r30, 1, 6
vertexLoop:
    rlwinm r29, r0, 30, 18, 25
    add r29, r29, r3
    psq_lux f10, r9, r11, 0, 7
    psq_lux f11, r9, r31, 1, 7
    add r9, r9, r12
    psq_l f0, 0x0(r29), 0, 0
    psq_l f1, 0x8(r29), 1, 0
    psq_l f6, 0x30(r29), 0, 0
    psq_l f7, 0x38(r29), 1, 0
    ps_muls0 f26, f0, f10
    ps_muls0 f27, f1, f10
    psq_l f2, 0x10(r29), 0, 0
    psq_l f3, 0x18(r29), 1, 0
    ps_madds0 f24, f0, f8, f6
    ps_madds0 f25, f1, f8, f7
    psq_l f4, 0x20(r29), 0, 0
    psq_l f5, 0x28(r29), 1, 0
    ps_madds1 f26, f2, f10, f26
    ps_madds1 f27, f3, f10, f27
    psq_lu f12, 0x2(r4), 0, 5
    ps_madds1 f24, f2, f8, f24
    ps_madds1 f25, f3, f8, f25
    ps_cmpo1 cr7, f12, f13
    ps_madds0 f26, f4, f11, f26
    ps_madds0 f27, f5, f11, f27
    ps_madds0 f24, f4, f9, f24
    ps_madds0 f25, f5, f9, f25
    ps_muls0 f26, f26, f12
    ps_muls0 f27, f27, f12
    ps_muls0 f24, f24, f12
    ps_muls0 f25, f25, f12
    ble cr7, storeVertex
    clrlslwi r29, r0, 24, 6
    add r29, r29, r3
    psq_l f0, 0x0(r29), 0, 0
    psq_l f1, 0x8(r29), 1, 0
    psq_l f6, 0x30(r29), 0, 0
    psq_l f7, 0x38(r29), 1, 0
    ps_muls0 f30, f0, f10
    ps_muls0 f31, f1, f10
    psq_l f2, 0x10(r29), 0, 0
    psq_l f3, 0x18(r29), 1, 0
    ps_madds0 f28, f0, f8, f6
    ps_madds0 f29, f1, f8, f7
    psq_l f4, 0x20(r29), 0, 0
    psq_l f5, 0x28(r29), 1, 0
    ps_madds1 f30, f2, f10, f30
    ps_madds1 f31, f3, f10, f31
    ps_madds1 f28, f2, f8, f28
    ps_madds1 f29, f3, f8, f29
    ps_madds0 f30, f4, f11, f30
    ps_madds0 f31, f5, f11, f31
    ps_madds0 f28, f4, f9, f28
    ps_madds0 f29, f5, f9, f29
    ps_madds1 f26, f30, f12, f26
    ps_madds1 f27, f31, f12, f27
    ps_madds1 f24, f28, f12, f24
    ps_madds1 f25, f29, f12, f25
storeVertex:
    lhzu r0, 0x2(r5)
    psq_lux f8, r8, r10, 0, 6
    psq_lux f9, r8, r30, 1, 6
    psq_stux f26, r7, r11, 0, 7
    psq_stux f27, r7, r31, 1, 7
    add r7, r7, r12
    psq_stux f24, r6, r10, 0, 6
    psq_stux f25, r6, r30, 1, 6
    bdnz vertexLoop
done:
    lwz r29, 0x8(r1)
    lwz r30, 0xc(r1)
    lwz r31, 0x10(r1)
    lfd f24, 0x18(r1)
    lfd f25, 0x20(r1)
    lfd f26, 0x28(r1)
    lfd f27, 0x30(r1)
    lfd f28, 0x38(r1)
    lfd f29, 0x40(r1)
    lfd f30, 0x48(r1)
    lfd f31, 0x50(r1)
    addi r1, r1, 0x58
    blr
} // 0x802ECCA4

/*
 * ==================== _rwDlSkinUpdate3WeightsP ====================
 * Skins the positions of vertices with three weights: per vertex three index bytes and three
 * weight bytes, each weight loaded as its own byte.
 */
// ORIGINAL-ASM: hand-written assembly (frame, paired-single schedule, one source line per instruction in the Gormiti line table)
asm void _rwDlSkinUpdate3WeightsP(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data) {
    nofralloc
    stwu r1, -0x18(r1)
    stfd f30, 0x8(r1)
    stfd f31, 0x10(r1)
    lwz r0, 0x1c(r5)
    cmpwi r0, 0x0
    mtctr r0
    ble done
    lwz r6, 0x0(r5)
    lwz r7, 0x8(r5)
    lwz r8, 0x10(r5)
    lwz r5, 0x28(r4)
    lwz r4, 0x24(r4)
    rotlwi r9, r8, 1
    subi r5, r5, 0x1
    subi r4, r4, 0x1
    subf r6, r8, r6
    subf r7, r8, r7
vertexLoop:
    ps_sub f0, f0, f0
    psq_lu f30, 0x1(r4), 1, 5
    psq_lu f31, 0x1(r4), 1, 5
    ps_merge00 f30, f30, f31
    psq_lu f31, 0x1(r4), 1, 5
    lbzu r10, 0x1(r5)
    lbzu r11, 0x1(r5)
    lbzu r12, 0x1(r5)
    ps_cmpo1 cr5, f30, f0
    ps_cmpo0 cr6, f31, f0
    psq_lux f8, r7, r8, 0, 6
    psq_lux f9, r7, r9, 1, 6
    clrlslwi r10, r10, 24, 6
    add r10, r10, r3
    psq_l f0, 0x0(r10), 0, 0
    psq_l f1, 0x8(r10), 1, 0
    psq_l f6, 0x30(r10), 0, 0
    psq_l f7, 0x38(r10), 1, 0
    psq_l f2, 0x10(r10), 0, 0
    psq_l f3, 0x18(r10), 1, 0
    ps_madds0 f10, f0, f8, f6
    ps_madds0 f11, f1, f8, f7
    psq_l f4, 0x20(r10), 0, 0
    psq_l f5, 0x28(r10), 1, 0
    ps_madds1 f10, f2, f8, f10
    ps_madds1 f11, f3, f8, f11
    ps_madds0 f10, f4, f9, f10
    ps_madds0 f11, f5, f9, f11
    ps_muls0 f10, f10, f30
    ps_muls0 f11, f11, f30
    ble cr5, storeVertex
    clrlslwi r11, r11, 24, 6
    add r11, r11, r3
    psq_l f0, 0x0(r11), 0, 0
    psq_l f1, 0x8(r11), 1, 0
    psq_l f6, 0x30(r11), 0, 0
    psq_l f7, 0x38(r11), 1, 0
    psq_l f2, 0x10(r11), 0, 0
    psq_l f3, 0x18(r11), 1, 0
    ps_madds0 f12, f0, f8, f6
    ps_madds0 f13, f1, f8, f7
    psq_l f4, 0x20(r11), 0, 0
    psq_l f5, 0x28(r11), 1, 0
    ps_madds1 f12, f2, f8, f12
    ps_madds1 f13, f3, f8, f13
    ps_madds0 f12, f4, f9, f12
    ps_madds0 f13, f5, f9, f13
    ps_madds1 f10, f12, f30, f10
    ps_madds1 f11, f13, f30, f11
    ble cr6, storeVertex
    clrlslwi r12, r12, 24, 6
    add r12, r12, r3
    psq_l f0, 0x0(r12), 0, 0
    psq_l f1, 0x8(r12), 1, 0
    psq_l f6, 0x30(r12), 0, 0
    psq_l f7, 0x38(r12), 1, 0
    psq_l f2, 0x10(r12), 0, 0
    psq_l f3, 0x18(r12), 1, 0
    ps_madds0 f12, f0, f8, f6
    ps_madds0 f13, f1, f8, f7
    psq_l f4, 0x20(r12), 0, 0
    psq_l f5, 0x28(r12), 1, 0
    ps_madds1 f12, f2, f8, f12
    ps_madds1 f13, f3, f8, f13
    ps_madds0 f12, f4, f9, f12
    ps_madds0 f13, f5, f9, f13
    ps_madds0 f10, f12, f31, f10
    ps_madds0 f11, f13, f31, f11
storeVertex:
    psq_stux f10, r6, r8, 0, 6
    psq_stux f11, r6, r9, 1, 6
    bdnz vertexLoop
done:
    lfd f30, 0x8(r1)
    lfd f31, 0x10(r1)
    addi r1, r1, 0x18
    blr
} // 0x802ECE78

/*
 * ==================== _rwDlSkinUpdate3WeightsPN ====================
 * Skins the positions and normals of vertices with three weights: per vertex three index bytes
 * and three weight bytes. The normals take the rotation only and skip nrmSkip bytes after each
 * vertex in both arrays.
 */
// ORIGINAL-ASM: hand-written assembly (frame, paired-single schedule, one source line per instruction in the Gormiti line table)
asm void _rwDlSkinUpdate3WeightsPN(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data) {
    nofralloc
    stwu r1, -0x58(r1)
    stw r27, 0x8(r1)
    stw r28, 0xc(r1)
    stw r29, 0x10(r1)
    stw r30, 0x14(r1)
    stw r31, 0x18(r1)
    stfd f25, 0x20(r1)
    stfd f26, 0x28(r1)
    stfd f27, 0x30(r1)
    stfd f28, 0x38(r1)
    stfd f29, 0x40(r1)
    stfd f30, 0x48(r1)
    stfd f31, 0x50(r1)
    lwz r0, 0x1c(r5)
    cmpwi r0, 0x0
    mtctr r0
    ble done
    lwz r6, 0x0(r5)
    lwz r7, 0x4(r5)
    lwz r8, 0x8(r5)
    lwz r9, 0xc(r5)
    lwz r10, 0x10(r5)
    lwz r11, 0x14(r5)
    lwz r12, 0x18(r5)
    lwz r5, 0x28(r4)
    lwz r4, 0x24(r4)
    rotlwi r27, r10, 1
    rotlwi r28, r11, 1
    subi r5, r5, 0x1
    subi r4, r4, 0x1
    subf r6, r10, r6
    subf r7, r11, r7
    subf r8, r10, r8
    subf r9, r11, r9
vertexLoop:
    ps_sub f0, f0, f0
    psq_lu f12, 0x1(r4), 1, 5
    psq_lu f13, 0x1(r4), 1, 5
    ps_merge00 f12, f12, f13
    psq_lu f13, 0x1(r4), 1, 5
    psq_lux f8, r8, r10, 0, 6
    psq_lux f9, r8, r27, 1, 6
    ps_cmpo1 cr5, f12, f0
    ps_cmpo0 cr6, f13, f0
    psq_lux f10, r9, r11, 0, 7
    psq_lux f11, r9, r28, 1, 7
    add r9, r9, r12
    lbzu r29, 0x1(r5)
    clrlslwi r29, r29, 24, 6
    add r29, r29, r3
    lbzu r30, 0x1(r5)
    clrlslwi r30, r30, 24, 6
    add r30, r30, r3
    lbzu r31, 0x1(r5)
    clrlslwi r31, r31, 24, 6
    add r31, r31, r3
    psq_l f0, 0x0(r29), 0, 0
    psq_l f1, 0x8(r29), 1, 0
    psq_l f6, 0x30(r29), 0, 0
    psq_l f7, 0x38(r29), 1, 0
    ps_muls0 f26, f0, f10
    ps_muls0 f27, f1, f10
    psq_l f2, 0x10(r29), 0, 0
    psq_l f3, 0x18(r29), 1, 0
    ps_madds0 f24, f0, f8, f6
    ps_madds0 f25, f1, f8, f7
    psq_l f4, 0x20(r29), 0, 0
    psq_l f5, 0x28(r29), 1, 0
    ps_madds1 f26, f2, f10, f26
    ps_madds1 f27, f3, f10, f27
    ps_madds1 f24, f2, f8, f24
    ps_madds1 f25, f3, f8, f25
    ps_madds0 f26, f4, f11, f26
    ps_madds0 f27, f5, f11, f27
    ps_madds0 f24, f4, f9, f24
    ps_madds0 f25, f5, f9, f25
    ps_muls0 f26, f26, f12
    ps_muls0 f27, f27, f12
    ps_muls0 f24, f24, f12
    ps_muls0 f25, f25, f12
    ble cr5, storeVertex
    psq_l f0, 0x0(r30), 0, 0
    psq_l f1, 0x8(r30), 1, 0
    psq_l f6, 0x30(r30), 0, 0
    psq_l f7, 0x38(r30), 1, 0
    ps_muls0 f30, f0, f10
    ps_muls0 f31, f1, f10
    psq_l f2, 0x10(r30), 0, 0
    psq_l f3, 0x18(r30), 1, 0
    ps_madds0 f28, f0, f8, f6
    ps_madds0 f29, f1, f8, f7
    psq_l f4, 0x20(r30), 0, 0
    psq_l f5, 0x28(r30), 1, 0
    ps_madds1 f30, f2, f10, f30
    ps_madds1 f31, f3, f10, f31
    ps_madds1 f28, f2, f8, f28
    ps_madds1 f29, f3, f8, f29
    ps_madds0 f30, f4, f11, f30
    ps_madds0 f31, f5, f11, f31
    ps_madds0 f28, f4, f9, f28
    ps_madds0 f29, f5, f9, f29
    ps_madds1 f26, f30, f12, f26
    ps_madds1 f27, f31, f12, f27
    ps_madds1 f24, f28, f12, f24
    ps_madds1 f25, f29, f12, f25
    ble cr6, storeVertex
    psq_l f0, 0x0(r31), 0, 0
    psq_l f1, 0x8(r31), 1, 0
    psq_l f6, 0x30(r31), 0, 0
    psq_l f7, 0x38(r31), 1, 0
    ps_muls0 f30, f0, f10
    ps_muls0 f31, f1, f10
    psq_l f2, 0x10(r31), 0, 0
    psq_l f3, 0x18(r31), 1, 0
    ps_madds0 f28, f0, f8, f6
    ps_madds0 f29, f1, f8, f7
    psq_l f4, 0x20(r31), 0, 0
    psq_l f5, 0x28(r31), 1, 0
    ps_madds1 f30, f2, f10, f30
    ps_madds1 f31, f3, f10, f31
    ps_madds1 f28, f2, f8, f28
    ps_madds1 f29, f3, f8, f29
    ps_madds0 f30, f4, f11, f30
    ps_madds0 f31, f5, f11, f31
    ps_madds0 f28, f4, f9, f28
    ps_madds0 f29, f5, f9, f29
    ps_madds0 f26, f30, f13, f26
    ps_madds0 f27, f31, f13, f27
    ps_madds0 f24, f28, f13, f24
    ps_madds0 f25, f29, f13, f25
storeVertex:
    psq_stux f26, r7, r11, 0, 7
    psq_stux f27, r7, r28, 1, 7
    add r7, r7, r12
    psq_stux f24, r6, r10, 0, 6
    psq_stux f25, r6, r27, 1, 6
    bdnz vertexLoop
done:
    lwz r27, 0x8(r1)
    lwz r28, 0xc(r1)
    lwz r29, 0x10(r1)
    lwz r30, 0x14(r1)
    lwz r31, 0x18(r1)
    lfd f25, 0x20(r1)
    lfd f26, 0x28(r1)
    lfd f27, 0x30(r1)
    lfd f28, 0x38(r1)
    lfd f29, 0x40(r1)
    lfd f30, 0x48(r1)
    lfd f31, 0x50(r1)
    addi r1, r1, 0x58
    blr
} // 0x802ECFE8

/*
 * ==================== _rwDlSkinUpdate4WeightsP ====================
 * Skins the positions of vertices with four weights: per vertex one word of indices (four bone
 * bytes) and four weight bytes, loaded as two pairs.
 */
// ORIGINAL-ASM: hand-written assembly (frame, paired-single schedule, one source line per instruction in the Gormiti line table)
asm void _rwDlSkinUpdate4WeightsP(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data) {
    nofralloc
    stwu r1, -0x18(r1)
    stfd f30, 0x8(r1)
    stfd f31, 0x10(r1)
    lwz r0, 0x1c(r5)
    cmpwi r0, 0x0
    mtctr r0
    ble done
    lwz r6, 0x0(r5)
    lwz r7, 0x8(r5)
    lwz r8, 0x10(r5)
    lwz r5, 0x28(r4)
    lwz r4, 0x24(r4)
    rotlwi r9, r8, 1
    subi r5, r5, 0x4
    subi r4, r4, 0x2
    subf r6, r8, r6
    subf r7, r8, r7
vertexLoop:
    ps_sub f0, f0, f0
    psq_lu f30, 0x2(r4), 0, 5
    psq_lu f31, 0x2(r4), 0, 5
    lwzu r0, 0x4(r5)
    psq_lux f8, r7, r8, 0, 6
    psq_lux f9, r7, r9, 1, 6
    ps_cmpo1 cr5, f30, f0
    ps_cmpo0 cr6, f31, f0
    ps_cmpo1 cr7, f31, f0
    rlwinm r10, r0, 14, 18, 25
    add r10, r10, r3
    psq_l f0, 0x0(r10), 0, 0
    psq_l f1, 0x8(r10), 1, 0
    psq_l f6, 0x30(r10), 0, 0
    psq_l f7, 0x38(r10), 1, 0
    psq_l f2, 0x10(r10), 0, 0
    psq_l f3, 0x18(r10), 1, 0
    ps_madds0 f10, f0, f8, f6
    ps_madds0 f11, f1, f8, f7
    psq_l f4, 0x20(r10), 0, 0
    psq_l f5, 0x28(r10), 1, 0
    ps_madds1 f10, f2, f8, f10
    ps_madds1 f11, f3, f8, f11
    ps_madds0 f10, f4, f9, f10
    ps_madds0 f11, f5, f9, f11
    ps_muls0 f10, f10, f30
    ps_muls0 f11, f11, f30
    ble cr5, storeVertex
    rlwinm r10, r0, 22, 18, 25
    add r10, r10, r3
    psq_l f0, 0x0(r10), 0, 0
    psq_l f1, 0x8(r10), 1, 0
    psq_l f6, 0x30(r10), 0, 0
    psq_l f7, 0x38(r10), 1, 0
    psq_l f2, 0x10(r10), 0, 0
    psq_l f3, 0x18(r10), 1, 0
    ps_madds0 f12, f0, f8, f6
    ps_madds0 f13, f1, f8, f7
    psq_l f4, 0x20(r10), 0, 0
    psq_l f5, 0x28(r10), 1, 0
    ps_madds1 f12, f2, f8, f12
    ps_madds1 f13, f3, f8, f13
    ps_madds0 f12, f4, f9, f12
    ps_madds0 f13, f5, f9, f13
    ps_madds1 f10, f12, f30, f10
    ps_madds1 f11, f13, f30, f11
    ble cr6, storeVertex
    rlwinm r10, r0, 30, 18, 25
    add r10, r10, r3
    psq_l f0, 0x0(r10), 0, 0
    psq_l f1, 0x8(r10), 1, 0
    psq_l f6, 0x30(r10), 0, 0
    psq_l f7, 0x38(r10), 1, 0
    psq_l f2, 0x10(r10), 0, 0
    psq_l f3, 0x18(r10), 1, 0
    ps_madds0 f12, f0, f8, f6
    ps_madds0 f13, f1, f8, f7
    psq_l f4, 0x20(r10), 0, 0
    psq_l f5, 0x28(r10), 1, 0
    ps_madds1 f12, f2, f8, f12
    ps_madds1 f13, f3, f8, f13
    ps_madds0 f12, f4, f9, f12
    ps_madds0 f13, f5, f9, f13
    ps_madds0 f10, f12, f31, f10
    ps_madds0 f11, f13, f31, f11
    ble cr7, storeVertex
    clrlslwi r10, r0, 24, 6
    add r10, r10, r3
    psq_l f0, 0x0(r10), 0, 0
    psq_l f1, 0x8(r10), 1, 0
    psq_l f6, 0x30(r10), 0, 0
    psq_l f7, 0x38(r10), 1, 0
    psq_l f2, 0x10(r10), 0, 0
    psq_l f3, 0x18(r10), 1, 0
    ps_madds0 f12, f0, f8, f6
    ps_madds0 f13, f1, f8, f7
    psq_l f4, 0x20(r10), 0, 0
    psq_l f5, 0x28(r10), 1, 0
    ps_madds1 f12, f2, f8, f12
    ps_madds1 f13, f3, f8, f13
    ps_madds0 f12, f4, f9, f12
    ps_madds0 f13, f5, f9, f13
    ps_madds1 f10, f12, f31, f10
    ps_madds1 f11, f13, f31, f11
storeVertex:
    psq_stux f10, r6, r8, 0, 6
    psq_stux f11, r6, r9, 1, 6
    bdnz vertexLoop
done:
    lfd f30, 0x8(r1)
    lfd f31, 0x10(r1)
    addi r1, r1, 0x18
    blr
} // 0x802ED23C

/*
 * ==================== _rwDlSkinUpdate4WeightsPN ====================
 * Skins the positions and normals of vertices with four weights: per vertex one word of indices
 * and four weight bytes. The normals take the rotation only and skip nrmSkip bytes after each
 * vertex in both arrays.
 */
// ORIGINAL-ASM: hand-written assembly (frame, paired-single schedule, one source line per instruction in the Gormiti line table)
asm void _rwDlSkinUpdate4WeightsPN(RwMatrix* matrixArray, RpSkin* skin, SkinAsmData* data) {
    nofralloc
    stwu r1, -0x50(r1)
    stw r29, 0x8(r1)
    stw r30, 0xc(r1)
    stw r31, 0x10(r1)
    stfd f25, 0x18(r1)
    stfd f26, 0x20(r1)
    stfd f27, 0x28(r1)
    stfd f28, 0x30(r1)
    stfd f29, 0x38(r1)
    stfd f30, 0x40(r1)
    stfd f31, 0x48(r1)
    lwz r0, 0x1c(r5)
    cmpwi r0, 0x0
    mtctr r0
    ble done
    lwz r6, 0x0(r5)
    lwz r7, 0x4(r5)
    lwz r8, 0x8(r5)
    lwz r9, 0xc(r5)
    lwz r10, 0x10(r5)
    lwz r11, 0x14(r5)
    lwz r12, 0x18(r5)
    lwz r5, 0x28(r4)
    lwz r4, 0x24(r4)
    rotlwi r29, r10, 1
    rotlwi r30, r11, 1
    subi r5, r5, 0x4
    subi r4, r4, 0x2
    subf r6, r10, r6
    subf r7, r11, r7
    subf r8, r10, r8
    subf r9, r11, r9
vertexLoop:
    ps_sub f0, f0, f0
    psq_lu f12, 0x2(r4), 0, 5
    psq_lu f13, 0x2(r4), 0, 5
    lwzu r0, 0x4(r5)
    psq_lux f8, r8, r10, 0, 6
    psq_lux f9, r8, r29, 1, 6
    ps_cmpo1 cr5, f12, f0
    ps_cmpo0 cr6, f13, f0
    ps_cmpo1 cr7, f13, f0
    psq_lux f10, r9, r11, 0, 7
    psq_lux f11, r9, r30, 1, 7
    add r9, r9, r12
    rlwinm r31, r0, 14, 18, 25
    add r31, r31, r3
    psq_l f0, 0x0(r31), 0, 0
    psq_l f1, 0x8(r31), 1, 0
    psq_l f6, 0x30(r31), 0, 0
    psq_l f7, 0x38(r31), 1, 0
    ps_muls0 f26, f0, f10
    ps_muls0 f27, f1, f10
    psq_l f2, 0x10(r31), 0, 0
    psq_l f3, 0x18(r31), 1, 0
    ps_madds0 f24, f0, f8, f6
    ps_madds0 f25, f1, f8, f7
    psq_l f4, 0x20(r31), 0, 0
    psq_l f5, 0x28(r31), 1, 0
    ps_madds1 f26, f2, f10, f26
    ps_madds1 f27, f3, f10, f27
    ps_madds1 f24, f2, f8, f24
    ps_madds1 f25, f3, f8, f25
    ps_madds0 f26, f4, f11, f26
    ps_madds0 f27, f5, f11, f27
    ps_madds0 f24, f4, f9, f24
    ps_madds0 f25, f5, f9, f25
    ps_muls0 f26, f26, f12
    ps_muls0 f27, f27, f12
    ps_muls0 f24, f24, f12
    ps_muls0 f25, f25, f12
    ble cr5, storeVertex
    rlwinm r31, r0, 22, 18, 25
    add r31, r31, r3
    psq_l f0, 0x0(r31), 0, 0
    psq_l f1, 0x8(r31), 1, 0
    psq_l f6, 0x30(r31), 0, 0
    psq_l f7, 0x38(r31), 1, 0
    ps_muls0 f30, f0, f10
    ps_muls0 f31, f1, f10
    psq_l f2, 0x10(r31), 0, 0
    psq_l f3, 0x18(r31), 1, 0
    ps_madds0 f28, f0, f8, f6
    ps_madds0 f29, f1, f8, f7
    psq_l f4, 0x20(r31), 0, 0
    psq_l f5, 0x28(r31), 1, 0
    ps_madds1 f30, f2, f10, f30
    ps_madds1 f31, f3, f10, f31
    ps_madds1 f28, f2, f8, f28
    ps_madds1 f29, f3, f8, f29
    ps_madds0 f30, f4, f11, f30
    ps_madds0 f31, f5, f11, f31
    ps_madds0 f28, f4, f9, f28
    ps_madds0 f29, f5, f9, f29
    ps_madds1 f26, f30, f12, f26
    ps_madds1 f27, f31, f12, f27
    ps_madds1 f24, f28, f12, f24
    ps_madds1 f25, f29, f12, f25
    ble cr6, storeVertex
    rlwinm r31, r0, 30, 18, 25
    add r31, r31, r3
    psq_l f0, 0x0(r31), 0, 0
    psq_l f1, 0x8(r31), 1, 0
    psq_l f6, 0x30(r31), 0, 0
    psq_l f7, 0x38(r31), 1, 0
    ps_muls0 f30, f0, f10
    ps_muls0 f31, f1, f10
    psq_l f2, 0x10(r31), 0, 0
    psq_l f3, 0x18(r31), 1, 0
    ps_madds0 f28, f0, f8, f6
    ps_madds0 f29, f1, f8, f7
    psq_l f4, 0x20(r31), 0, 0
    psq_l f5, 0x28(r31), 1, 0
    ps_madds1 f30, f2, f10, f30
    ps_madds1 f31, f3, f10, f31
    ps_madds1 f28, f2, f8, f28
    ps_madds1 f29, f3, f8, f29
    ps_madds0 f30, f4, f11, f30
    ps_madds0 f31, f5, f11, f31
    ps_madds0 f28, f4, f9, f28
    ps_madds0 f29, f5, f9, f29
    ps_madds0 f26, f30, f13, f26
    ps_madds0 f27, f31, f13, f27
    ps_madds0 f24, f28, f13, f24
    ps_madds0 f25, f29, f13, f25
    ble cr7, storeVertex
    clrlslwi r31, r0, 24, 6
    add r31, r31, r3
    psq_l f0, 0x0(r31), 0, 0
    psq_l f1, 0x8(r31), 1, 0
    psq_l f6, 0x30(r31), 0, 0
    psq_l f7, 0x38(r31), 1, 0
    ps_muls0 f30, f0, f10
    ps_muls0 f31, f1, f10
    psq_l f2, 0x10(r31), 0, 0
    psq_l f3, 0x18(r31), 1, 0
    ps_madds0 f28, f0, f8, f6
    ps_madds0 f29, f1, f8, f7
    psq_l f4, 0x20(r31), 0, 0
    psq_l f5, 0x28(r31), 1, 0
    ps_madds1 f30, f2, f10, f30
    ps_madds1 f31, f3, f10, f31
    ps_madds1 f28, f2, f8, f28
    ps_madds1 f29, f3, f8, f29
    ps_madds0 f30, f4, f11, f30
    ps_madds0 f31, f5, f11, f31
    ps_madds0 f28, f4, f9, f28
    ps_madds0 f29, f5, f9, f29
    ps_madds1 f26, f30, f13, f26
    ps_madds1 f27, f31, f13, f27
    ps_madds1 f24, f28, f13, f24
    ps_madds1 f25, f29, f13, f25
storeVertex:
    psq_stux f26, r7, r11, 0, 7
    psq_stux f27, r7, r30, 1, 7
    add r7, r7, r12
    psq_stux f24, r6, r10, 0, 6
    psq_stux f25, r6, r29, 1, 6
    bdnz vertexLoop
done:
    lwz r29, 0x8(r1)
    lwz r30, 0xc(r1)
    lwz r31, 0x10(r1)
    lfd f25, 0x18(r1)
    lfd f26, 0x20(r1)
    lfd f27, 0x28(r1)
    lfd f28, 0x30(r1)
    lfd f29, 0x38(r1)
    lfd f30, 0x40(r1)
    lfd f31, 0x48(r1)
    addi r1, r1, 0x50
    blr
} // 0x802ED3EC
