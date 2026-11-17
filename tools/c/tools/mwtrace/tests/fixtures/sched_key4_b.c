/* mwtrace sched fixture: two independent definitions, p = table (lis, addi) before i = 0 (li), passed to a call, in a
 * function that is not the first of its file (warm_up takes the first-function quirk). sched_key4_a.c has the two
 * statements the other way round. */
extern int table[10];
extern void g(int, int *);
void warm_up(int a) { g(a, 0); }
void probe(int n) {
    int i;
    int *p;
    p = table;
    i = 0;
    g(i, p);
    g(n, p);
}
