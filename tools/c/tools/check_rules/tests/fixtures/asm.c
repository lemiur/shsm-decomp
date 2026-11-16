asm void fa(void) {
    nofralloc
    lis r3, x@ha
    blr
}
void fb(void) {
    asm { mr r3, r4 }
    __asm__ volatile ("sync");
    INCLUDE_ASM("x", fb);
    __emit(0x7c0004ac);
}
static asm int fc(register int a) { mr r3, a; blr }
__asm void fd() {}
