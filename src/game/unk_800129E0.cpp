// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800129E0.h>

/*
====================
fn_800129E0
====================
*/
// 0x800129E0
int fn_800129E0() {
    int enabled = fn_800161D0();
    if ( enabled ) {
        int first = fn_800F997C();
        int second = fn_800F9988();
        // 0x20000: first argument of the sample, meaning not yet evidenced
        fn_80015358( 0x20000, 1 );
        fn_80015358( first, 10 );
        fn_80015358( second, 10 );
        fn_8001539C( 3 );
    }
    return enabled;
}

/*
====================
fn_80012A64
====================
*/
// 0x80012A64
void fn_80012A64() {
    fn_800161DC();
}
