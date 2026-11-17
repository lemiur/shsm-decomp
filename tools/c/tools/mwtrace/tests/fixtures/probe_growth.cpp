// mwtrace's real-tree test compiles this with the flags of game/CDrawingTest: ten expansions of heavy take grow's
// cost past 2000 and four times its cost before, so the call-graph visitor undoes all of them.
inline int heavy(int a) {
    a = a * 1 + (a >> 3);
    a = a * 2 + (a >> 3);
    a = a * 3 + (a >> 3);
    a = a * 4 + (a >> 3);
    a = a * 5 + (a >> 3);
    a = a * 6 + (a >> 3);
    a = a * 7 + (a >> 3);
    a = a * 8 + (a >> 3);
    a = a * 9 + (a >> 3);
    a = a * 10 + (a >> 3);
    a = a * 11 + (a >> 3);
    a = a * 12 + (a >> 3);
    a = a * 13 + (a >> 3);
    a = a * 14 + (a >> 3);
    a = a * 15 + (a >> 3);
    a = a * 16 + (a >> 3);
    a = a * 17 + (a >> 3);
    a = a * 18 + (a >> 3);
    a = a * 19 + (a >> 3);
    a = a * 20 + (a >> 3);
    a = a * 21 + (a >> 3);
    a = a * 22 + (a >> 3);
    a = a * 23 + (a >> 3);
    a = a * 24 + (a >> 3);
    a = a * 25 + (a >> 3);
    a = a * 26 + (a >> 3);
    a = a * 27 + (a >> 3);
    a = a * 28 + (a >> 3);
    a = a * 29 + (a >> 3);
    a = a * 30 + (a >> 3);
    a = a * 31 + (a >> 3);
    a = a * 32 + (a >> 3);
    a = a * 33 + (a >> 3);
    a = a * 34 + (a >> 3);
    a = a * 35 + (a >> 3);
    a = a * 36 + (a >> 3);
    a = a * 37 + (a >> 3);
    a = a * 38 + (a >> 3);
    a = a * 39 + (a >> 3);
    a = a * 40 + (a >> 3);
    a = a * 41 + (a >> 3);
    a = a * 42 + (a >> 3);
    a = a * 43 + (a >> 3);
    a = a * 44 + (a >> 3);
    a = a * 45 + (a >> 3);
    a = a * 46 + (a >> 3);
    a = a * 47 + (a >> 3);
    a = a * 48 + (a >> 3);
    a = a * 49 + (a >> 3);
    a = a * 50 + (a >> 3);
    a = a * 51 + (a >> 3);
    a = a * 52 + (a >> 3);
    a = a * 53 + (a >> 3);
    a = a * 54 + (a >> 3);
    a = a * 55 + (a >> 3);
    a = a * 56 + (a >> 3);
    a = a * 57 + (a >> 3);
    a = a * 58 + (a >> 3);
    a = a * 59 + (a >> 3);
    a = a * 60 + (a >> 3);
    return a;
}
int grow(int x) {
    return heavy(x + 0) + heavy(x + 1) + heavy(x + 2) + heavy(x + 3) + heavy(x + 4) + heavy(x + 5) + heavy(x + 6) + heavy(x + 7) + heavy(x + 8) + heavy(x + 9);
}
