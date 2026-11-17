/* mwtrace's real-tree test compiles this with the flags of revolution/os/OSNet without "-ipa file" (inlining per
 * function, bottom-up) and without "-inline noauto": early's call of later comes before later's definition (not yet
 * a candidate), after's is expanded under the bottom-up size limit. */
static int later(int x);
int early(int x) { return later(x) + 1; }
static int later(int x) { return x * 2; }
int after(int x) { return later(x) + 2; }
