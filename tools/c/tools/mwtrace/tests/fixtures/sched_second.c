/* mwtrace sched fixture: the same probe as sched_first.c, after warm_up, which takes the first-function quirk (its
 * parameter's copy is substituted and survives), so probe's copy is deleted before scheduling. */
extern int table[10];
extern int *gp;
extern int gi;
extern void g(int);
void warm_up(int a) { g(a + 1); }
void probe(int n) {
    gi = n;
    gp = table;
}
