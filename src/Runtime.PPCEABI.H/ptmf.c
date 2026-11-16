/*
 * __ptmf_scall
 *
 * Calls a pointer to member function: r12 points at the PTMF record
 * (this-adjustment, vtable offset, function or vtable-slot offset) and r3 is
 * the object; a negative vtable offset means a direct function address.
 */
asm void __ptmf_scall(void) {
    nofralloc
    lwz r0, 0x0(r12)
    lwz r11, 0x4(r12)
    lwz r12, 0x8(r12)
    add r3, r3, r0
    cmpwi r11, 0x0
    blt direct
    lwzx r12, r3, r12
    lwzx r12, r12, r11
direct:
    mtctr r12
    bctr
}
