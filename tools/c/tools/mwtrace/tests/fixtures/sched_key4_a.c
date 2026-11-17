/* mwtrace sched fixture: two independent definitions, i = 0 (li) before p = table (lis, addi), passed to a call, in a
 * function that is not the first of its file (warm_up takes the first-function quirk). sched_key4_b.c has the two
 * statements the other way round. */
extern int table[10];
extern void g(int, int *);
void warm_up(int a) { g(a, 0); }
void probe(int n) {
    int i;
    int *p;
    i = 0;
    p = table;
    g(i, p);
    g(n, p);
}
