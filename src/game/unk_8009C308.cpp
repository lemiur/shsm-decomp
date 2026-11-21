// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C308.h>

/*
====================
fn_8009C308
====================
*/
// Name and role invented. Resumes the video decode thread when its start flag is set; the thread stays suspended
// until the flag is raised, so the call is a no-op before the thread is created.
// 0x8009C308
extern "C" void fn_8009C308(void) {
    if (lbl_805297A0 != 0) {
        OSResumeThread(&lbl_8046D4D0.thread);
    }
}
