// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <revolution/os/OSThread.h>
#include <revolution/thp/__thp.h>

/*
 * fn_8009C014
 *
 * Resumes the THP read thread once CreateReadThread has started it (the flag is set there).
 */
void fn_8009C014(void) {
    if (lbl_80529798) {
        OSResumeThread(&lbl_8046C1B8.thread);
    }
} // 0x8009C014
