/* Unit name unconfirmed: __stdio_atexit, MSL misc_io.c by recall; the filename is a guess. */
#include <MSL_C/MSL_Common/abort_exit.h>
#include <MSL_C/MSL_Common/ansi_files.h>
#include <MSL_C/MSL_Common/misc_io.h>

/*
 * __stdio_atexit
 *
 * Registers the stream cleanup (__close_all) to run at exit; called the
 * first time a console stream is written.
 */
void __stdio_atexit(void) {
    __stdio_exit = __close_all;
} // 0x80199A14
