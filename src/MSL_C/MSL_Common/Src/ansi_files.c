/* Unit name unconfirmed: the standard streams table and the close/flush-all walkers, MSL ansi_files.c by recall; the filename is a guess. */
#include <stdio.h>
#include <stdlib.h>
#include <MSL_C/MSL_Common/ansi_files.h>
#include <MSL_C/MSL_Common/console_io.h>

#define __console_buffer_size 0x100

static unsigned char stdin_buff[__console_buffer_size];
static unsigned char stdout_buff[__console_buffer_size];
static unsigned char stderr_buff[__console_buffer_size];

FILE __files[4] = {
    /* stdin */
    {
        0,
        {__must_exist, __read, __line_buffered, __console_file, 0, 0},
        {0, 0, 0, 0},
        0, 0, 0, {0, 0}, {0, 0},
        0,
        stdin_buff, __console_buffer_size, stdin_buff, 0, 0, 0, 0,
        0, __read_console, __write_console, __close_console, 0,
        &__files[1],
    },
    /* stdout */
    {
        1,
        {__must_exist, __write, __line_buffered, __console_file, 0, 0},
        {0, 0, 0, 0},
        0, 0, 0, {0, 0}, {0, 0},
        0,
        stdout_buff, __console_buffer_size, stdout_buff, 0, 0, 0, 0,
        0, __read_console, __write_console, __close_console, 0,
        &__files[2],
    },
    /* stderr */
    {
        2,
        {__must_exist, __write, __no_buffer, __console_file, 0, 0},
        {0, 0, 0, 0},
        0, 0, 0, {0, 0}, {0, 0},
        0,
        stderr_buff, __console_buffer_size, stderr_buff, 0, 0, 0, 0,
        0, __read_console, __write_console, __close_console, 0,
        &__files[3],
    },
    /* spare slot, claimed by the first fopen */
    {0},
};

/*
 * __close_all
 *
 * Closes every open stream at exit and releases the dynamically allocated
 * FILE structures. The statically allocated ones are marked unavailable.
 */
void __close_all(void) {
    FILE* p = __files;
    FILE* plast;

    while (p != NULL) {
        if (p->file_mode.file_kind != __closed_file) {
            fclose(p);
        }

        plast = p;
        p = p->next_file_struct;

        if (plast->is_dynamically_allocated) {
            free(plast);
        } else {
            plast->file_mode.file_kind = __unavailable_file;

            if (p != NULL && p->is_dynamically_allocated) {
                plast->next_file_struct = NULL;
            }
        }
    }
} // 0x80197160

/*
 * __flush_all
 *
 * Flushes every open stream; EOF if any flush failed.
 */
int __flush_all(void) {
    int result = 0;
    FILE* p = __files;

    while (p != NULL) {
        if (p->file_mode.file_kind != __closed_file) {
            if (fflush(p)) {
                result = EOF;
            }
        }

        p = p->next_file_struct;
    }

    return result;
} // 0x80197204
