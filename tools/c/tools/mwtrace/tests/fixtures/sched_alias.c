/* mwtrace sched fixture: memory dependences. copy_plain reads through a plain pointer parameter, which may alias the
 * static dst (may_alias 1: each store waits for the loads before it); copy_const reads through a pointer to const
 * (may_alias 0: the loads are free of the stores). */
typedef struct { int a, b, c, d; } S;
static S dst;
extern void g(int);
void warm_up(int a) { g(a + 1); }
void copy_plain(S *p) { dst = *p; }
void copy_const(const S *p) { dst = *p; }
