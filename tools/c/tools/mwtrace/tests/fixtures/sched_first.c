/* mwtrace sched fixture: the first-function quirk. probe is the first function of the file, so the entry copy of its
 * parameter (mr r32, r3) is substituted into its use but survives copy propagation into scheduler pass 1, and is
 * deleted only after it. sched_second.c has the same probe after a function that takes the quirk. */
extern int table[10];
extern int *gp;
extern int gi;
void probe(int n) {
    gi = n;
    gp = table;
}
