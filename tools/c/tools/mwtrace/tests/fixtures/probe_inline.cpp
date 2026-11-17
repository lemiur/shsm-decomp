// mwtrace's real-tree test compiles this with the flags of game/CDrawingTest (C++, -inline auto, -ipa file) and
// checks the inliner's rule for each call: auto-inline by statement count (15 fits, 16 does not), the inline
// keyword, varargs (kind 0), never_inline, a statement-bodied callee, and a recursion cut by the level-3 cost limit.
static int leaf(int x) { return x + 1; }
inline int kw(int x) { return x * 3; }
static int big(int x) {
    int a = x;
    a += 1; a += 2; a += 3; a += 4; a += 5; a += 6; a += 7; a += 8;
    a += 9; a += 10; a += 11; a += 12; a += 13; a += 14;
    return a;
}
static int big15(int x) {
    int a = x;
    a += 1; a += 2; a += 3; a += 4; a += 5; a += 6; a += 7; a += 8;
    a += 9; a += 10; a += 11; a += 12; a += 13;
    return a;
}
static int va(int n, ...) { return n; }
inline int vakw(int n, ...) { return n; }
static int rec(int n) { return n ? rec(n - 1) : 0; }
__attribute__((never_inline)) static int ni(int x) { return x - 1; }
extern int ext(int);
static int stmtk(int x) { int r = 0; for (int i = 0; i < x; i++) r += ext(i); return r; }
int caller1(int x) { return leaf(x) + kw(x) + big(x) + big15(x) + va(x) + vakw(x) + rec(x) + ni(x) + stmtk(x); }
int caller2(int x) { int y = stmtk(x); y += leaf(y); return y; }
