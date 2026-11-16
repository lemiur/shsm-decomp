void f(void) {
    volatile u32 a; // volatile: delay loop, zeldaret/tp os.c
    // volatile: hw register
    vu16 *reg;
    volatile int *const p, q = 0;
    for (a = 0; a < 1000; a++) {}
}
u8 *p; // byteptr: allocator header
int pad0; /* pad: alignment */
int x; // RULE-EXCEPTION: R4, reason
void g(void) {
} // no copy in retail (dead-stripped)
void h(void) {
}   // no copy in retail (inlined)
